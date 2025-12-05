/*
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This file is part of the device-mapper multipath userspace tools.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/socket.h"
#include "libutils/dtoa.h"
#include "libxson/tree.h"

#include <poll.h>

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

#define DEFAULT_SOCKET "@/org/kernel/linux/storage/multipathd"
#define MAX_REPLY_LEN (32 * 1024 * 1024)

enum {
    FAM_MULTIPATHD_UP,
    FAM_MULTIPATHD_MAP_STATE,
    FAM_MULTIPATHD_MAP_PATHS,
    FAM_MULTIPATHD_MAP_PATH_FAULTS,
    FAM_MULTIPATHD_PATH_GROUP_STATE,
    FAM_MULTIPATHD_PATH_STATE,
    FAM_MULTIPATHD_PATH_DEVICE_STATE,
    FAM_MULTIPATHD_PATH_CHECK_STATE,
    FAM_MULTIPATHD_MAX
};

static metric_family_t fams[FAM_MULTIPATHD_MAX] = {
    [FAM_MULTIPATHD_UP] = {
        .name = "multipathd_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the multipathd daemon be reached.",
    },
    [FAM_MULTIPATHD_MAP_STATE] = {
        .name = "multipathd_map_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Multipath map state",
    },
    [FAM_MULTIPATHD_MAP_PATHS] = {
        .name = "multipathd_map_paths",
        .type = METRIC_TYPE_GAUGE,
        .help = "Multipath map number of paths.",
    },
    [FAM_MULTIPATHD_MAP_PATH_FAULTS] = {
        .name = "multipathd_map_path_faults",
        .type = METRIC_TYPE_GAUGE,
        .help = "Total number of paths failures in Multipath map.",
    },
    [FAM_MULTIPATHD_PATH_GROUP_STATE] = {
        .name = "multipathd_path_group_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Multipath path group state",
    },
    [FAM_MULTIPATHD_PATH_STATE] = {
        .name = "multipathd_path_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Multipath path state",
    },
    [FAM_MULTIPATHD_PATH_DEVICE_STATE] = {
        .name = "multipathd_path_device_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Multipath path device state",
    },
    [FAM_MULTIPATHD_PATH_CHECK_STATE] = {
        .name = "multipathd_path_check_state",
        .type = METRIC_TYPE_GAUGE,
        .help = "Multipath path check state",
    }
};

static cdtime_t g_timeout = 0;

static ssize_t mpath_read(int fd, void *buf, size_t len, cdtime_t timeout)
{
	size_t total = 0;

    cdtime_t start = cdtime();

	while (len) {
    	struct pollfd pfd = {
		    .fd = fd,
		    .events = POLLIN
        };

        cdtime_t elapsed = cdtime() - start;
        if (elapsed >= timeout) {
            PLUGIN_ERROR("Timeout waiting for response.");
			return -1;
        }

		int ret = poll(&pfd, 1, CDTIME_T_TO_MS(timeout - elapsed));
		if (!ret) {
            PLUGIN_ERROR("Timeout waiting for response.");
			return -1;
		} else if (ret < 0) {
			if (errno == EINTR)
				continue;
            PLUGIN_ERROR("Error polling for response: %s", STRERRNO);
			return -1;
		} else if (!(pfd.revents & POLLIN)) {
			continue;
        }

		ssize_t n = recv(fd, buf, len, 0);
		if (n < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
            PLUGIN_ERROR("Error reading response: %s", STRERRNO);
			return -1;
		}

		if (n == 0)
			return total;

		buf = n + (char *)buf;
		len -= n;
		total += n;
	}

	return total;
}

static size_t mpath_write(int fd, const void *buf, size_t len)
{
	size_t total = 0;

	while (len) {
		ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
		if (n < 0) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
            PLUGIN_ERROR("Error send request: %s", STRERRNO);
			return total;
		}
		if (n == 0)
			return total;

		buf = n + (const char *)buf;
		len -= n;
		total += n;
	}

	return total;
}

static char *mpath_recv_reply(int fd, cdtime_t timeout)
{
	size_t len = 0;

	ssize_t ret = mpath_read(fd, &len, sizeof(len), timeout);
	if (ret < 0)
		return NULL;

	if (ret != sizeof(len)) {
        PLUGIN_ERROR("Unexpected response size, expected %zu got: %zu.", sizeof(len), ret);
		return NULL;
	}

	if (len <= 0 || len >= MAX_REPLY_LEN) {
        PLUGIN_ERROR("Response size (%zu) is greather than max reply size (%d).",
                     len, MAX_REPLY_LEN);
		return NULL;
	}

	char *reply = malloc(len);
	if (reply == NULL) {
        PLUGIN_ERROR("malloc failed.");   
		return NULL;
    }

	ret = mpath_read(fd, reply, len, timeout);
	if (ret < 0) {
		free(reply);
		return NULL;
    }

	if ((size_t)ret != len) {
        PLUGIN_ERROR("Got less bytes (%zi) than expected (%zu).", ret, len);
		free(reply);
		return NULL;
	}

	reply[len - 1] = '\0';
	return reply;
}

int mpath_send_cmd(int fd, const char *cmd)
{
	size_t len = strlen(cmd) + 1;

	if (mpath_write(fd, &len, sizeof(len)) != sizeof(len))
		return -1;

	if (mpath_write(fd, cmd, len) != len)
		return -1;

	return 0;
}

static void multipathd_parse_path(xson_value_t *path, char *map_name, char *map_uuid,
                                                      char *group_id)
{
    if (path == NULL)
        return;

    if (!xson_value_is_object(path))
        return;

    char *dev = NULL;
    char *dev_st = NULL;
    char *dm_st = NULL;
    char *chk_st = NULL;

    for (size_t i = 0; i < xson_value_object_size(path); i++) {
        xson_keyval_t *kv = xson_value_object_at(path, i);
        char *key = xson_keyval_key(kv);
        xson_value_t *value = xson_keyval_value(kv);
        if ((key == NULL) || (value == NULL))
            break;

        switch(key[0]) {
        case 'c':
            if (strcmp(key, "chk_st") == 0)
                chk_st = xson_value_get_string(value);
            break;
        case 'd':
            if (strcmp(key, "dev_st") == 0) {
                dev_st = xson_value_get_string(value);
            } else if (strcmp(key, "dev") == 0) {
                dev = xson_value_get_string(value);
            } else if (strcmp(key, "dm_st") == 0) {
                dm_st = xson_value_get_string(value);
            }
            break;
        }
    }

    if (dev == NULL)
        return;

    if (dev_st != NULL) {
        state_t states[] = {
            { .name = "running", .enabled = false },
            { .name = "offline", .enabled = false },
            { .name = "unknown", .enabled = false },
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        size_t j;
        for (j = 0; j < set.num ; j++) {
            if (strncmp(set.ptr[j].name, dev_st, strlen(set.ptr[j].name)) == 0) {
                set.ptr[j].enabled = true;
                break;
            }
        }
        if (j == set.num)
            set.ptr[set.num - 1].enabled = true;

        metric_family_append(&fams[FAM_MULTIPATHD_PATH_DEVICE_STATE], VALUE_STATE_SET(set), NULL,
                             &(label_pair_const_t){.name="map_name", .value=map_name},
                             &(label_pair_const_t){.name="map_uuid", .value=map_uuid},
                             &(label_pair_const_t){.name="group_id", .value=group_id},
                             &(label_pair_const_t){.name="device",   .value=dev},
                             NULL);
    }

    if (chk_st != NULL) {
        state_t states[] = {
            { .name = "ready",       .enabled = false },
            { .name = "faulty",      .enabled = false },
            { .name = "shaky",       .enabled = false },
            { .name = "ghost",       .enabled = false },
            { .name = "i/o pending", .enabled = false },
            { .name = "i/o timeout", .enabled = false },
            { .name = "delayed",     .enabled = false },
            { .name = "undef",       .enabled = false }
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        size_t j;
        for (j = 0; j < set.num ; j++) {
            if (strncmp(set.ptr[j].name, chk_st, strlen(set.ptr[j].name)) == 0) {
                set.ptr[j].enabled = true;
                break;
            }
        }
        if (j == set.num)
            set.ptr[set.num - 1].enabled = true;

        metric_family_append(&fams[FAM_MULTIPATHD_PATH_CHECK_STATE], VALUE_STATE_SET(set), NULL,
                             &(label_pair_const_t){.name="map_name", .value=map_name},
                             &(label_pair_const_t){.name="map_uuid", .value=map_uuid},
                             &(label_pair_const_t){.name="group_id", .value=group_id},
                             &(label_pair_const_t){.name="device",   .value=dev},
                             NULL);
    }

    if (dm_st != NULL) {
        state_t states[] = {
            { .name = "active", .enabled = false },
            { .name = "failed", .enabled = false },
            { .name = "undef",  .enabled = false }
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        size_t j;
        for (j = 0; j < set.num ; j++) {
            if (strncmp(set.ptr[j].name, dm_st, strlen(set.ptr[j].name)) == 0) {
                set.ptr[j].enabled = true;
                break;
            }
        }
        if (j == set.num)
            set.ptr[set.num - 1].enabled = true;

        metric_family_append(&fams[FAM_MULTIPATHD_PATH_STATE], VALUE_STATE_SET(set), NULL,
                             &(label_pair_const_t){.name="map_name", .value=map_name},
                             &(label_pair_const_t){.name="map_uuid", .value=map_uuid},
                             &(label_pair_const_t){.name="group_id", .value=group_id},
                             &(label_pair_const_t){.name="device",   .value=dev},
                             NULL);
    }

}

static void multipathd_parse_path_group(xson_value_t *path_group, char *map_name, char *map_uuid)
{
    if (path_group == NULL)
        return;

    if (!xson_value_is_object(path_group))
        return;

    char *dm_st = NULL;
    double group = NAN;
    xson_value_t *paths = NULL;

    for (size_t i = 0; i < xson_value_object_size(path_group); i++) {
        xson_keyval_t *kv = xson_value_object_at(path_group, i);
        char *key = xson_keyval_key(kv);
        xson_value_t *value = xson_keyval_value(kv);
        if ((key == NULL) || (value == NULL))
            break;

        switch(key[0]) {
        case 'd':
            if (strcmp(key, "dm_st") == 0)
                dm_st = xson_value_get_string(value);
            break;
        case 'p':
            if (strcmp(key, "paths") == 0)
                paths = value;
            break;
        case 'g':
            if (strcmp(key, "group") == 0)
                group = xson_value_get_number(value);
            break;
        }
    }

    if (group == NAN)
        return;

    char group_id[DTOA_MAX];
    dtoa(group, group_id, sizeof(group_id));

    if (dm_st != NULL) {
        state_t states[] = {
            { .name = "enabled",  .enabled = false },
            { .name = "disabled", .enabled = false },
            { .name = "active",   .enabled = false },
            { .name = "undef",    .enabled = false },
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        size_t j;
        for (j = 0; j < set.num ; j++) {
            if (strncmp(set.ptr[j].name, dm_st, strlen(set.ptr[j].name)) == 0) {
                set.ptr[j].enabled = true;
                break;
            }
        }
        if (j == set.num)
            set.ptr[set.num - 1].enabled = true;

        metric_family_append(&fams[FAM_MULTIPATHD_PATH_GROUP_STATE], VALUE_STATE_SET(set), NULL,
                             &(label_pair_const_t){.name="map_name", .value=map_name},
                             &(label_pair_const_t){.name="map_uuid", .value=map_uuid},
                             &(label_pair_const_t){.name="group_id", .value=group_id},
                             NULL);
    }

    if (paths == NULL)
        return;

    if (xson_value_is_object(paths)) {
        multipathd_parse_path(paths, map_name, map_uuid, group_id);
    } else if (xson_value_is_array(paths)) {
        for (size_t i = 0; i < xson_value_array_size(paths); i++) {
            multipathd_parse_path(xson_value_array_at(paths, i), map_name, map_uuid, group_id);
        }
    }

}

static void multipathd_parse_map(xson_value_t *map)
{
    if (map == NULL)
        return;

    if (!xson_value_is_object(map))
        return;

    char *name = NULL;
    char *uuid = NULL;
    char *dm_st = NULL;
    double paths = NAN;
    double path_faults = NAN;
    xson_value_t *path_groups = NULL;

    for (size_t i = 0; i < xson_value_object_size(map); i++) {
        xson_keyval_t *kv = xson_value_object_at(map, i);
        char *key = xson_keyval_key(kv);
        xson_value_t *value = xson_keyval_value(kv);
        if ((key == NULL) || (value == NULL))
            break;
        switch (key[0]) {
        case 'n':
            if (strcmp(key, "name") == 0)
                name = xson_value_get_string(value);
            break;
        case 'u':
            if (strcmp(key, "uuid") == 0)
                uuid = xson_value_get_string(value);
            break;
        case 'p':
            if (strcmp(key, "path_groups") == 0) {
                path_groups = value;
            } else if (strcmp(key, "paths") == 0) {
                paths = xson_value_get_number(value);
            } else if (strcmp(key, "path_faults") == 0) {
                path_faults = xson_value_get_number(value);
            }
            break;
        case 'd':
            if (strcmp(key, "dm_st") == 0)
                dm_st = xson_value_get_string(value);
            break;
        }
    }

    if ((name == NULL) || (uuid == NULL))
        return;

    if (dm_st != NULL) {
        state_t states[] = {
            { .name = "suspend", .enabled = false },
            { .name = "active",  .enabled = false },
            { .name = "undef",   .enabled = false },
        };
        state_set_t set = { .num = STATIC_ARRAY_SIZE(states), .ptr = states };

        size_t j;
        for (j = 0; j < set.num ; j++) {
            if (strncmp(set.ptr[j].name, dm_st, strlen(set.ptr[j].name)) == 0) {
                set.ptr[j].enabled = true;
                break;
            }
        }
        if (j == set.num)
            set.ptr[set.num - 1].enabled = true;

        metric_family_append(&fams[FAM_MULTIPATHD_MAP_STATE], VALUE_STATE_SET(set), NULL,
                             &(label_pair_const_t){.name="name", .value=name},
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             NULL);
    }

    if (path_faults != NAN) {
        metric_family_append(&fams[FAM_MULTIPATHD_MAP_PATH_FAULTS], VALUE_GAUGE(path_faults), NULL,
                             &(label_pair_const_t){.name="name", .value=name},
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             NULL);
    }

    if (paths != NAN) {
        metric_family_append(&fams[FAM_MULTIPATHD_MAP_PATHS], VALUE_GAUGE(paths), NULL,
                             &(label_pair_const_t){.name="name", .value=name},
                             &(label_pair_const_t){.name="uuid", .value=uuid},
                             NULL);
    }

    if (path_groups == NULL)
        return;

    if (xson_value_is_object(path_groups)) {
        multipathd_parse_path_group(path_groups, name, uuid);
    } else if (xson_value_is_array(path_groups)) {
        for (size_t i = 0; i < xson_value_array_size(path_groups); i++) {
            multipathd_parse_path_group(xson_value_array_at(path_groups, i), name, uuid);
        }
    }
}

static int multipathd_read(void)
{
    if (g_timeout == 0)
        g_timeout = plugin_get_interval()/2;

    int fd = socket_connect_unix_stream(DEFAULT_SOCKET, 0);
    if (fd < 0) {
        metric_family_append(&fams[FAM_MULTIPATHD_UP], VALUE_GAUGE(0), NULL, NULL);
        plugin_dispatch_metric_family(&fams[FAM_MULTIPATHD_UP], 0);
        return -1;
    }

    if (mpath_send_cmd(fd, "show maps json") != 0) {
        close(fd);
        metric_family_append(&fams[FAM_MULTIPATHD_UP], VALUE_GAUGE(0), NULL, NULL);
        plugin_dispatch_metric_family(&fams[FAM_MULTIPATHD_UP], 0);
		return -1;
    }

    char *reply = mpath_recv_reply(fd, g_timeout);
    if (reply == NULL) {
        close(fd);
        metric_family_append(&fams[FAM_MULTIPATHD_UP], VALUE_GAUGE(0), NULL, NULL);
        plugin_dispatch_metric_family(&fams[FAM_MULTIPATHD_UP], 0);
        return -1;
    }

    close(fd);

    char error[256] = {'\0'};
    xson_value_t *root = xson_tree_parser(reply, error, sizeof(error));
    if (root == NULL) {
        PLUGIN_ERROR("Error parsing json: %s", error);
        close(fd);
        free(reply);
        metric_family_append(&fams[FAM_MULTIPATHD_UP], VALUE_GAUGE(0), NULL, NULL);
        plugin_dispatch_metric_family(&fams[FAM_MULTIPATHD_UP], 0);
        return -1;
    }

    free(reply);

    xson_value_t *maps = xson_value_object_find(root, "maps");
    if (xson_value_is_object(maps)) {
        multipathd_parse_map(maps);
    } else if (xson_value_is_array(maps)) {
        for (size_t i = 0; i < xson_value_array_size(maps); i++) {
            multipathd_parse_map(xson_value_array_at(maps, i));
        }
    }

    xson_value_free(root);

    metric_family_append(&fams[FAM_MULTIPATHD_UP], VALUE_GAUGE(1), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_MULTIPATHD_MAX, 0);

    return 0;
}

static int multipathd_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &g_timeout);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("multipathd", multipathd_config);
    plugin_register_read("multipathd", multipathd_read);
}
