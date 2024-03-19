// SPDX-License-Identifier: GPL-2.0-only or MIT
// SPDX-FileCopyrightText: Copyright (C) 2007-2008 C-Ware, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2008-2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2013 Kris Nielander
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Kris Nielander <nielander at fox-it.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Luke Heberling <lukeh at c-ware.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    char *file;
    FILE *fh;
    struct stat stat;
    bool whole;
    bool force_rewind;
    plugin_match_t *matches;
} tail_t;

static int tail_close(tail_t *tail)
{
    if (tail->fh == NULL)
        return 0;

    fclose(tail->fh);
    tail->fh = NULL;
    return 0;
}

static int tail_reopen(tail_t *tail)
{
    struct stat stat_buf = {0};
    /* coverity[TOCTOU] */
    int status = stat(tail->file, &stat_buf);
    if (status != 0) {
        PLUGIN_ERROR("Stat '%s' failed: %s", tail->file, STRERRNO);
        return -1;
    }

    /* The file is already open.. */
    if ((tail->fh != NULL) && (stat_buf.st_ino == tail->stat.st_ino)) {
        /* Seek to the beginning if file was truncated */
        if (stat_buf.st_size < tail->stat.st_size) {
            PLUGIN_INFO("File '%s' was truncated.", tail->file);
            status = fseek(tail->fh, 0, SEEK_SET);
            if (status != 0) {
                PLUGIN_ERROR("fseek '%s' failed: %s", tail->file, STRERRNO);
                fclose(tail->fh);
                tail->fh = NULL;
                return -1;
            }
        }
        memcpy(&tail->stat, &stat_buf, sizeof(struct stat));
        return 1;
    }

    /* Unless flag for rewinding to the start is set, seek to the end
     * if we re-open the same file again or the file opened is the first at all
     * or the first after an error */
    int seek_end = 0;
    if ((tail->stat.st_ino == 0) || (tail->stat.st_ino == stat_buf.st_ino))
        seek_end = !tail->force_rewind;

    FILE *fh = fopen(tail->file, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Cannot open '%s': %s", tail->file, STRERRNO);
        return -1;
    }

    if (seek_end != 0) {
        status = fseek(fh, 0, SEEK_END);
        if (status != 0) {
            PLUGIN_ERROR("fseek '%s' failed: %s", tail->file, STRERRNO);
            fclose(fh);
            return -1;
        }
    }

    if (tail->fh != NULL)
        fclose(tail->fh);
    tail->fh = fh;
    memcpy(&tail->stat, &stat_buf, sizeof(struct stat));

    return 0;
}

static int tail_readline(tail_t *tail, char *buf, size_t buflen)
{
    if (tail->fh == NULL) {
        int status = tail_reopen(tail);
        if (status < 0)
            return status;
    }
    assert(tail->fh != NULL);

    /* Try to read from the filehandle. If that succeeds, everything appears to
     * be fine and we can return. */
    clearerr(tail->fh);
    if (fgets(buf, buflen, tail->fh) != NULL) {
        buf[buflen - 1] = '\0';
        return 0;
    }

    /* Check if we encountered an error */
    if (ferror(tail->fh) != 0) {
        /* Jupp, error. Force `cu_tail_reopen' to reopen the file.. */
        fclose(tail->fh);
        tail->fh = NULL;
    }
    /* else: eof -> check if the file was moved away and reopen the new file if
     * so.. */

    int status = tail_reopen(tail);
    /* error -> return with error */
    if (status < 0) {
        return status;
    /* file end reached and file not reopened -> nothing more to read */
    } else if (status > 0) {
        buf[0] = 0;
        return 0;
    }

    /* If we get here: file was re-opened and there may be more to read.. Let's
     * try again. */
    if (fgets(buf, buflen, tail->fh) != NULL) {
        buf[buflen - 1] = '\0';
        return 0;
    }

    if (ferror(tail->fh) != 0) {
        PLUGIN_WARNING("fgets '%s' returned an error: %s", tail->file, STRERRNO);
        fclose(tail->fh);
        tail->fh = NULL;
        return -1;
    }

    /* EOf, well, apparently the new file is empty.. */
    buf[0] = 0;
    return 0;
}

static int tail_read(user_data_t *ud)
{
    tail_t *tail = ud->data;

    while (true) {
        char buf[8192];
        int status = tail_readline(tail, buf, sizeof(buf));
        if (status != 0) {
            PLUGIN_ERROR("File '%s': tail_readline failed with status %i.", tail->file, status);
            return -1;
        }

        /* check for EOF */
        if (buf[0] == 0)
          break;

        size_t len = strlen(buf);
        while (len > 0) {
            if (buf[len - 1] != '\n')
                break;
            buf[len - 1] = '\0';
            len--;
        }

        if (len == 0)
            continue;

        status = plugin_match(tail->matches, buf);
        if (status != 0)
            PLUGIN_WARNING("plugin_match failed.");
    }

    if (tail->whole)
        tail_close(tail);

    plugin_match_dispatch(tail->matches, true);

    return 0;
}

static void tail_destroy(void *arg)
{
    tail_t *tail = arg;
    if (tail == NULL)
        return;

    if (tail->fh != NULL)
        fclose(tail->fh);

    free(tail->file);

    if (tail->matches != NULL)
        plugin_match_shutdown(tail->matches);

    free(tail);
    return;
}

static int tail_config_file(config_item_t *ci)
{
    cdtime_t interval = 0;

    char *file = NULL;
    int status = cf_util_get_string(ci, &file);
    if (status != 0)
        return status;

    tail_t *tail  = calloc(1, sizeof(*tail));
    if (tail == NULL)
        return -1;
    tail->file = file;
    tail->whole = false;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("whole", child->key) == 0) {
            status = cf_util_get_boolean(child, &tail->whole);
        } else if (strcasecmp("match", child->key) == 0) {
            status = plugin_match_config(child, &tail->matches);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        if (tail->matches == NULL) {
            PLUGIN_ERROR("No (valid) 'match' block ");
            status = -1;
        }
    }

    if (status != 0) {
        tail_destroy(tail);
        return -1;
    }

    return plugin_register_complex_read("tail", tail->file, tail_read, interval,
                                        &(user_data_t){.data=tail, .free_func=tail_destroy});
}

static int tail_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("file", child->key) == 0) {
            status = tail_config_file(child);
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
    plugin_register_config("tail", tail_config);
}
