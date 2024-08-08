// SPDX-License-Identifier: GPL-2.0-only OR ISC
// SPDX-FileCopyrightText: Copyright (C) 2017 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/time.h"

#include "taskstats.h"

#include <libmnl/libmnl.h>
#include <linux/genetlink.h>
#include <linux/taskstats.h>

struct ts_s {
    struct mnl_socket *nl;
    pid_t pid;
    uint32_t seq;
    uint16_t genl_id_taskstats;
    unsigned int port_id;
};

/* nlmsg_errno returns the errno encoded in nlh or zero if not an error. */
static int nlmsg_errno(struct nlmsghdr *nlh, size_t sz)
{
    if (!mnl_nlmsg_ok(nlh, (int)sz)) {
        PLUGIN_ERROR("mnl_nlmsg_ok failed.");
        return EPROTO;
    }

    if (nlh->nlmsg_type != NLMSG_ERROR)
        return 0;

    struct nlmsgerr *nlerr = mnl_nlmsg_get_payload(nlh);
    /* (struct nlmsgerr).error holds a negative errno. */
    return nlerr->error * (-1);
}

static int get_taskstats_attr_cb(const struct nlattr *attr, void *data)
{
    struct taskstats *ret_taskstats = data;

    uint16_t type = mnl_attr_get_type(attr);
    switch (type) {
    case TASKSTATS_TYPE_STATS:
        if (mnl_attr_get_payload_len(attr) != sizeof(*ret_taskstats)) {
            PLUGIN_ERROR("mnl_attr_get_payload_len(attr) = %" PRIu32 ", want %zu",
                          mnl_attr_get_payload_len(attr), sizeof(*ret_taskstats));
            return MNL_CB_ERROR;
        }
        struct taskstats *ts = mnl_attr_get_payload(attr);
        memmove(ret_taskstats, ts, sizeof(*ret_taskstats));
        return MNL_CB_OK;
        break;
    case TASKSTATS_TYPE_AGGR_PID: /* fall through */
    case TASKSTATS_TYPE_AGGR_TGID:
        return mnl_attr_parse_nested(attr, get_taskstats_attr_cb, ret_taskstats);
        break;
    case TASKSTATS_TYPE_PID: /* fall through */
    case TASKSTATS_TYPE_TGID:
        /* ignore */
        return MNL_CB_OK;
        break;
    default:
        PLUGIN_DEBUG("unknown attribute %" PRIu16
                     ", want one of TASKSTATS_TYPE_AGGR_PID/TGID, TASKSTATS_TYPE_STATS", type);
    }
    return MNL_CB_OK;
}

static int get_taskstats_msg_cb(const struct nlmsghdr *nlh, void *data)
{
    return mnl_attr_parse(nlh, sizeof(struct genlmsghdr), get_taskstats_attr_cb, data);
}

static int get_taskstats(ts_t *ts, uint32_t tgid, struct taskstats *ret_taskstats)
{
    char buffer[MNL_SOCKET_BUFFER_SIZE];
    uint32_t seq = ts->seq++;

    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buffer);
    *nlh = (struct nlmsghdr){
        .nlmsg_len = nlh->nlmsg_len,
        .nlmsg_type = ts->genl_id_taskstats,
        .nlmsg_flags = NLM_F_REQUEST,
        .nlmsg_seq = seq,
        .nlmsg_pid = ts->pid,
    };

    struct genlmsghdr *genh = mnl_nlmsg_put_extra_header(nlh, sizeof(*genh));
    *genh = (struct genlmsghdr){
        .cmd = TASKSTATS_CMD_GET,
        .version = TASKSTATS_GENL_VERSION, // or TASKSTATS_VERSION?
    };

    // mnl_attr_put_u32(nlh, TASKSTATS_CMD_ATTR_PID, tgid);
    mnl_attr_put_u32(nlh, TASKSTATS_CMD_ATTR_TGID, tgid);

    if (mnl_socket_sendto(ts->nl, nlh, nlh->nlmsg_len) < 0) {
        int status = errno;
        PLUGIN_ERROR("mnl_socket_sendto() = %s", STRERROR(status));
        return status;
    }

    int status = mnl_socket_recvfrom(ts->nl, buffer, sizeof(buffer));
    if (status < 0) {
        status = errno;
        PLUGIN_ERROR("mnl_socket_recvfrom() = %s", STRERROR(status));
        return status;
    } else if (status == 0) {
        PLUGIN_ERROR("mnl_socket_recvfrom() = 0");
        return ECONNABORTED;
    }
    size_t buffer_size = (size_t)status;

    if ((status = nlmsg_errno((void *)buffer, buffer_size)) != 0) {
        PLUGIN_ERROR("TASKSTATS_CMD_GET(TASKSTATS_CMD_ATTR_TGID = %" PRIu32 ") = %s",
                     (uint32_t)tgid, STRERROR(status));
        return status;
    }

    status = mnl_cb_run(buffer, buffer_size, seq, ts->port_id,
                        get_taskstats_msg_cb, ret_taskstats);
    if (status < MNL_CB_STOP) {
        PLUGIN_ERROR("Parsing message failed.");
        return EPROTO;
    }

    return 0;
}

static int get_family_id_attr_cb(const struct nlattr *attr, void *data)
{
    uint16_t type = mnl_attr_get_type(attr);
    if (type != CTRL_ATTR_FAMILY_ID)
        return MNL_CB_OK;

    if (mnl_attr_validate(attr, MNL_TYPE_U16) < 0) {
        ERROR("mnl_attr_validate() = %s", STRERRNO);
        return MNL_CB_ERROR;
    }

    uint16_t *ret_family_id = data;
    *ret_family_id = mnl_attr_get_u16(attr);
    return MNL_CB_STOP;
}

static int get_family_id_msg_cb(const struct nlmsghdr *nlh, void *data)
{
    return mnl_attr_parse(nlh, sizeof(struct genlmsghdr), get_family_id_attr_cb, data);
}

/* get_family_id initializes ts->genl_id_taskstats. Returns 0 on success and
 * an error code otherwise. */
static int get_family_id(ts_t *ts)
{
    char buffer[MNL_SOCKET_BUFFER_SIZE];
    uint32_t seq = ts->seq++;

    struct nlmsghdr *nlh = mnl_nlmsg_put_header(buffer);
    *nlh = (struct nlmsghdr){
        .nlmsg_len = nlh->nlmsg_len,
        .nlmsg_type = GENL_ID_CTRL,
        .nlmsg_flags = NLM_F_REQUEST,
        .nlmsg_seq = seq,
        .nlmsg_pid = ts->pid,
    };

    struct genlmsghdr *genh = mnl_nlmsg_put_extra_header(nlh, sizeof(*genh));
    *genh = (struct genlmsghdr){
        .cmd = CTRL_CMD_GETFAMILY,
        .version = 0x01,
    };

    mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, TASKSTATS_GENL_NAME);

    assert(genh->cmd == CTRL_CMD_GETFAMILY);
    assert(genh->version == TASKSTATS_GENL_VERSION);

    if (mnl_socket_sendto(ts->nl, nlh, nlh->nlmsg_len) < 0) {
        int status = errno;
        PLUGIN_ERROR("mnl_socket_sendto() = %s", STRERROR(status));
        return status;
    }

    ts->genl_id_taskstats = 0;
    while (true) {
        int status = mnl_socket_recvfrom(ts->nl, buffer, sizeof(buffer));
        if (status < 0) {
            status = errno;
            PLUGIN_ERROR("mnl_socket_recvfrom() = %s", STRERROR(status));
            return status;
        } else if (status == 0) {
            break;
        }
        size_t buffer_size = (size_t)status;

        if ((status = nlmsg_errno((void *)buffer, buffer_size)) != 0) {
            PLUGIN_ERROR("CTRL_CMD_GETFAMILY(\"%s\"): %s", TASKSTATS_GENL_NAME, STRERROR(status));
            return status;
        }

        status = mnl_cb_run(buffer, buffer_size, seq, ts->port_id,
                            get_family_id_msg_cb, &ts->genl_id_taskstats);
        if (status < MNL_CB_STOP) {
            PLUGIN_ERROR("Parsing message failed.");
            return EPROTO;
        } else if (status == MNL_CB_STOP) {
            break;
        }
    }

    if (ts->genl_id_taskstats == 0) {
        PLUGIN_ERROR("Netlink communication succeeded, but genl_id_taskstats is still zero.");
        return ENOENT;
    }

    return 0;
}

void ts_destroy(ts_t *ts)
{
    if (ts == NULL)
        return;

    if (ts->nl != NULL) {
        mnl_socket_close(ts->nl);
        ts->nl = NULL;
    }

    free(ts);
}

ts_t *ts_create(void)
{
    ts_t *ts = calloc(1, sizeof(*ts));
    if (ts == NULL) {
        PLUGIN_ERROR("calloc failed: %s", STRERRNO);
        return NULL;
    }

    if ((ts->nl = mnl_socket_open(NETLINK_GENERIC)) == NULL) {
        PLUGIN_ERROR("mnl_socket_open(NETLINK_GENERIC) = %s", STRERRNO);
        ts_destroy(ts);
        return NULL;
    }

    if (mnl_socket_bind(ts->nl, 0, MNL_SOCKET_AUTOPID) != 0) {
        PLUGIN_ERROR("mnl_socket_bind() = %s", STRERRNO);
        ts_destroy(ts);
        return NULL;
    }

    ts->pid = getpid();
    ts->port_id = mnl_socket_get_portid(ts->nl);

    int status = get_family_id(ts);
    if (status != 0) {
        PLUGIN_ERROR("get_family_id() = %s", STRERROR(status));
        ts_destroy(ts);
        return NULL;
    }

    return ts;
}

int ts_delay_by_tgid(ts_t *ts, uint32_t tgid, ts_delay_t *out)
{
    if ((ts == NULL) || (out == NULL))
        return EINVAL;

    struct taskstats raw = {0};

    int status = get_taskstats(ts, tgid, &raw);
    if (status != 0)
        return status;

    out->cpu_ns = raw.cpu_delay_total,
    out->blkio_ns = raw.blkio_delay_total,
    out->swapin_ns = raw.swapin_delay_total,
    out->freepages_ns = raw.freepages_delay_total,
#if TASKSTATS_VERSION >= 14
    out->irq_ns = raw.irq_delay_total;
#endif

    return 0;
}
