// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2011  New Dream Network
// SPDX-FileCopyrightText: Copyright (C) 2015  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Colin McCabe <cmccabe at alumni.cmu.edu>
// SPDX-FileContributor: Dennis Zou <yunzou at cisco.com>
// SPDX-FileContributor: Dan Ryder <daryder at cisco.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libxson/json_parse.h"

#include <poll.h>
#include <sys/un.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

#define RETRY_AVGCOUNT -1

#define RETRY_ON_EINTR(ret, expr)    \
    while (1) {                      \
        ret = expr;                  \
        if (ret >= 0)                \
            break;                   \
        ret = -errno;                \
        if (ret != -EINTR)           \
            break;                   \
    }

/** Timeout interval in seconds */
#define CEPH_TIMEOUT_INTERVAL 1

typedef enum {
    PERF_COUNTER_NONE       = 0x00,
    PERF_COUNTER_TIME       = 0x01, /* float (measuring seconds) */
    PERF_COUNTER_U64        = 0x02, /* integer (note: either TIME or U64 *must* be set) */
    PERF_COUNTER_LONGRUNAVG = 0x04, /* paired counter + sum (time) */
    PERF_COUNTER_COUNTER    = 0x08, /* counter (vs gauge) */
    PERF_COUNTER_HISTOGRAM  = 0x10, /* histogram (vector) of values */
} perf_counter_type_t;

typedef enum {
    PERF_METRIC_NONE,
    PERF_METRIC_COUNTER,
    PERF_METRIC_GAUGE,
    PERF_METRIC_LONGRUNAVG,
    PERF_METRIC_HISTOGRAM,
    PERF_METRIC_XXX // FIXME
} perf_metric_t;

typedef enum {
    PERF_LONGRUN_NONE     = -1,
    PERF_LONGRUN_AVGCOUNT =  0,
    PERF_LONGRUN_SUM      =  1,
    PERF_LONGRUN_AVGTIME  =  2,
    PERF_LONGRUN_MAX      =  3
} perf_logrun_t;

typedef struct {
    char *name;
    int type;
    perf_metric_t perf_metric;
    char *metric;
    char *metric_longrun[PERF_LONGRUN_MAX];
    char *description;
} perf_value_t;

typedef enum {
    PERF_VALUE_NONE,
    PERF_VALUE_TYPE,
    PERF_VALUE_DESCRIPTION
} perf_value_type_t;

typedef struct {
    char *name;
    c_avl_tree_t *tree;
} perf_key_t;

/** Track state and handler while parsing JSON */
typedef struct {
    c_avl_tree_t *tree;
    label_set_t *labels;

    cdtime_t time;
    perf_key_t *perf_key;
    perf_value_t *perf_value;
    perf_value_type_t perf_value_type;
    perf_logrun_t perf_longrun;

    size_t depth;
} json_struct_t;

typedef struct {
    uint32_t version; /* version of the admin_socket interface */
    char *name;       /* daemon name */
    char *asok_path;  /* path to the socket to talk to the ceph daemon */
    cdtime_t timeout;
    label_set_t labels;
    bool have_schema;
    c_avl_tree_t *schema;
} ceph_daemon_t;

typedef enum {
    CSTATE_UNCONNECTED = 0,
    CSTATE_WRITE_REQUEST,
    CSTATE_READ_VERSION,
    CSTATE_READ_AMT,
    CSTATE_READ_JSON,
} cstate_t;

typedef enum {
    ASOK_REQ_VERSION = 0,
    ASOK_REQ_DATA = 1,
    ASOK_REQ_SCHEMA = 2,
    ASOK_REQ_NONE = 1000,
} request_type_t;

typedef struct {
    ceph_daemon_t *d;         /* The Ceph daemon that we're talking to */
    uint32_t request_type;    /* Request type */
    cstate_t state;           /* The connection state */
    int asok;                 /* The socket we use to talk to this daemon */
    uint32_t amt;             /* The amount of data remaining to read / write. */
    uint32_t json_len;        /* Length of the JSON to read */
    unsigned char *json;      /* Buffer containing JSON data */
    json_struct_t yajl;       /* Keep data important to yajl processing */
} ceph_conn_t;

extern const char valid_metric_chars[256];

static char *metric_pair(char *prefix, char *suffix)
{
    size_t len_prefix = strlen(prefix);
    size_t len_suffix = strlen(suffix);

    char *metric = malloc(len_prefix + len_suffix + 2);
    if (metric == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return NULL;
    }

    if (valid_metric_chars[(int)prefix[0]] != 1)
        metric[0] = '_';
    else
        metric[0] = prefix[0];

    for (size_t i=1; i < len_prefix; i++) {
        if (valid_metric_chars[(int)prefix[i]] == 0)
            metric[i] = '_';
        else
            metric[i] = prefix[i];
    }

    metric[len_prefix] = '_';

    for (size_t i=0; i < len_suffix; i++) {
        if (valid_metric_chars[(int)suffix[i]] == 0)
            metric[i + len_prefix + 1] = '_';
        else
            metric[i + len_prefix + 1] = suffix[i];
    }

    metric[len_prefix + len_suffix + 1] = '\0';

    return metric;
}

static void ceph_perf_value_free(perf_value_t *perf_value)
{
    if (perf_value == NULL)
        return;

    free(perf_value->name);
    free(perf_value->metric);
    for (size_t i=0; i < PERF_LONGRUN_MAX; i++) {
        free(perf_value->metric_longrun[i]);
    }
    free(perf_value->description);
    free(perf_value);
}

static void ceph_perf_key_free(perf_key_t *perf_key)
{
    if (perf_key == NULL)
        return;

    if (perf_key->tree != NULL) {
        while (true) {
            perf_value_t *perf_value = NULL;
            char *value = NULL;
            int status = c_avl_pick(perf_key->tree, (void *)&value, (void *)&perf_value);
            if (status != 0)
                break;
            ceph_perf_value_free(perf_value);
        }
        c_avl_destroy(perf_key->tree);
    }

    free(perf_key->name);
    free(perf_key);
}

static bool ceph_schema_cb_number(void *ctx, const char *number_val, size_t number_len)
{
    json_struct_t *state = (json_struct_t *)ctx;

    if ((state->depth == 3) && (state->perf_value_type == PERF_VALUE_TYPE)) {
        if (state->perf_value == NULL) {
            PLUGIN_ERROR("perf_value is NULL.");
            return false;
        }

        char num[64] = "";
        if (number_len > sizeof(num)-1)
            return 1;
        memcpy(num, number_val, number_len);
        num[number_len] = '\0';

        state->perf_value->type = atoi(num);

        if (state->perf_value->type & PERF_COUNTER_LONGRUNAVG) {
            state->perf_value->perf_metric = PERF_METRIC_LONGRUNAVG;
//            if (state->perf_value->type & PERF_COUNTER_TIME) {
//            f->dump_string("value_type", "real-integer-pair");
//            } else {
//            f->dump_string("value_type", "integer-integer-pair");
//            }
        } else if (state->perf_value->type & PERF_COUNTER_HISTOGRAM) {
            state->perf_value->perf_metric = PERF_METRIC_HISTOGRAM;
//            if (state->perf_value->type & PERF_COUNTER_TIME) {
//            f->dump_string("value_type", "real-2d-histogram");
//            } else {
//            f->dump_string("value_type", "integer-2d-histogram");
//            }
        } else {
            if (state->perf_value->type & PERF_COUNTER_TIME) {
                state->perf_value->perf_metric = PERF_METRIC_GAUGE;
            } else {
                state->perf_value->perf_metric = PERF_METRIC_COUNTER;
//                if (state->perf_value->type & PERF_COUNTER_COUNTER)
//                else
//                    state->perf_value->perf_metric = PERF_METRIC_XXX;
            }
        }
        state->perf_value->metric = metric_pair(state->perf_key->name, state->perf_value->name);

        if ((state->perf_value->metric != NULL) &&
            (state->perf_value->perf_metric == PERF_METRIC_LONGRUNAVG)) {
            state->perf_value->metric_longrun[PERF_LONGRUN_AVGCOUNT] =
                metric_pair(state->perf_value->metric, "avgcount");
            state->perf_value->metric_longrun[PERF_LONGRUN_SUM] =
                metric_pair(state->perf_value->metric, "sum");
            state->perf_value->metric_longrun[PERF_LONGRUN_AVGTIME] =
                metric_pair(state->perf_value->metric,  "avgtime");
        }

#if 0

  4 add_u64_counter           PERFCOUNTER_U64 | PERFCOUNTER_COUNTER
  5 add_u64_avg               PERFCOUNTER_U64 | PERFCOUNTER_LONGRUNAVG
  6 add_time                  PERFCOUNTER_TIME
  7 add_time_avg              PERFCOUNTER_TIME | PERFCOUNTER_LONGRUNAVG
  8 add_u64_counter_histogram PERFCOUNTER_U64 | PERFCOUNTER_HISTOGRAM | PERFCOUNTER_COUNTER

     if (d->type & PERFCOUNTER_COUNTER) {
       f->dump_string("metric_type", "counter");
     } else {
       f->dump_string("metric_type", "gauge");
     }

     if (d->type & PERFCOUNTER_LONGRUNAVG) {
       if (d->type & PERFCOUNTER_TIME) {
         f->dump_string("value_type", "real-integer-pair");
       } else {
         f->dump_string("value_type", "integer-integer-pair");
       }
     } else if (d->type & PERFCOUNTER_HISTOGRAM) {
       if (d->type & PERFCOUNTER_TIME) {
         f->dump_string("value_type", "real-2d-histogram");
       } else {
         f->dump_string("value_type", "integer-2d-histogram");
       }
     } else {
       if (d->type & PERFCOUNTER_TIME) {
         f->dump_string("value_type", "real");
       } else {
         f->dump_string("value_type", "integer");
       }
     }
#endif

    }

    return true;
}

static bool ceph_schema_cb_string(void *ctx, const char *string_val, size_t string_len)
{
    json_struct_t *state = (json_struct_t *)ctx;

    if ((state->depth == 3) && (state->perf_value_type == PERF_VALUE_DESCRIPTION)) {
        if (state->perf_value == NULL) {
            PLUGIN_ERROR("perf_value is NULL.");
            return false;
        }

        if (state->perf_value->description != NULL)
            free(state->perf_value->description);

        state->perf_value->description = strndup((const char *)string_val, string_len);
        if (state->perf_value->description == NULL) {
            PLUGIN_ERROR("strndup failed.");
            return false;
        }
    }

    return true;
}

static bool ceph_schema_cb_start_map(void *ctx)
{
    json_struct_t *state = (json_struct_t *)ctx;

    /* Push key to the stack */
    if (state->depth == JSON_MAX_DEPTH)
        return false;

    state->depth++;

    switch (state->depth) {
    case 1:
        state->perf_key = NULL;
        state->perf_value = NULL;
        state->perf_value_type = PERF_VALUE_NONE;
        break;
    case 2:
        state->perf_value = NULL;
        state->perf_value_type = PERF_VALUE_NONE;
        break;
    case 3:
        state->perf_value_type = PERF_VALUE_NONE;
        break;
    }

    return true;
}

static bool ceph_schema_cb_end_map(void *ctx)
{
    json_struct_t *state = (json_struct_t *)ctx;

    /* Pop key from the stack */
    if (state->depth == 0)
        return false;

    state->depth--;

    switch (state->depth) {
    case 1:
        state->perf_key = NULL;
        state->perf_value = NULL;
        state->perf_value_type = PERF_VALUE_NONE;
        break;
    case 2:
        state->perf_value = NULL;
        state->perf_value_type = PERF_VALUE_NONE;
        break;
    case 3:
        state->perf_value_type = PERF_VALUE_NONE;
        break;
    }

    return true;
}

static bool ceph_schema_cb_map_key(void *ctx, const char *key, size_t string_len)
{
    json_struct_t *state = (json_struct_t *)ctx;

    if (state->tree == NULL) { // FIXME
        state->tree = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (state->tree == NULL)
            return false;
    }

    switch (state->depth) {
    case 1: {
        char *name = strndup((const char *)key, string_len);
        if (name == NULL) {
            PLUGIN_ERROR("strndup failed.");
            state->perf_key = NULL;
            return false;
        }

        perf_key_t *perf_key = calloc(1, sizeof(*perf_key));
        if (perf_key == NULL) {
            PLUGIN_ERROR("calloc failed.");
            free(name);
            state->perf_key = NULL;
            return false;
        }

        perf_key->name = name;

        perf_key->tree = c_avl_create((int (*)(const void *, const void *))strcmp);
        if (perf_key->tree == NULL) {
            ceph_perf_key_free(perf_key);
            state->perf_key = NULL;
            return false;
        }
        int status = c_avl_insert(state->tree, perf_key->name, perf_key);
        if (status != 0) {
            ceph_perf_key_free(perf_key);
            state->perf_key = NULL;
            return false;
        }
        state->perf_key = perf_key;
    }   break;
    case 2: {
        if (state->perf_key == NULL) {
            PLUGIN_ERROR("perf_key is NULL.");
            state->perf_value = NULL;
            return false;
        }

        char *name = strndup((const char *)key, string_len);
        if (name == NULL) {
            PLUGIN_ERROR("strndup failed.");
            state->perf_value = NULL;
            return false;
        }

        perf_value_t *perf_value = calloc(1, sizeof(*perf_value));
        if (perf_value == NULL) {
            PLUGIN_ERROR("calloc failed.");
            free(name);
            state->perf_value = NULL;
            return false;
        }

        perf_value->name = name;

        int status = c_avl_insert(state->perf_key->tree, perf_value->name, perf_value);
        if (status != 0) {
            ceph_perf_value_free(perf_value);
            state->perf_value = NULL;
            return false;
        }

        state->perf_value = perf_value;
    }   break;
    case 3:
        state->perf_value_type = PERF_VALUE_NONE;
        switch (string_len) {
        case 4:
            if (strncmp((const char *)key, "type", string_len) == 0)
                state->perf_value_type = PERF_VALUE_TYPE;
            break;
        case 11:
            if (strncmp((const char *)key, "description", string_len) == 0)
                state->perf_value_type = PERF_VALUE_DESCRIPTION;
            break;
        }
        break;
    }

    return true;
}

static json_callbacks_t schema_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = ceph_schema_cb_number,
    .json_string      = ceph_schema_cb_string,
    .json_start_map   = ceph_schema_cb_start_map,
    .json_map_key     = ceph_schema_cb_map_key,
    .json_end_map     = ceph_schema_cb_end_map,
    .json_start_array = NULL,
    .json_end_array   = NULL,
};

static bool ceph_data_cb_number(void *ctx, const char *number_val, size_t number_len)
{
    json_struct_t *state = (json_struct_t *)ctx;

    char num[64] = "";
    if (number_len > sizeof(num)-1)
        return true;
    memcpy(num, number_val, number_len);
    num[number_len] = '\0';

    switch(state->depth) {
    case 2:
        if (state->perf_value != NULL) {
            if (state->perf_value->perf_metric == PERF_METRIC_GAUGE) {
                metric_family_t fam = {
                    .name = state->perf_value->metric,
                    .type = METRIC_TYPE_GAUGE,
                    .help = state->perf_value->description,
                };
                metric_family_append(&fam, VALUE_GAUGE(atof(num)), state->labels, NULL);
                plugin_dispatch_metric_family(&fam, state->time);
            } else if (state->perf_value->perf_metric == PERF_METRIC_COUNTER) {
                metric_family_t fam = {
                    .name = state->perf_value->metric,
                    .type = METRIC_TYPE_COUNTER,
                    .help = state->perf_value->description,
                };
                metric_family_append(&fam, VALUE_COUNTER(atoi(num)), state->labels, NULL);
                plugin_dispatch_metric_family(&fam, state->time);
            }
        }
        break;
    case 3:
        if (state->perf_value != NULL) {
            switch (state->perf_longrun) {
            case PERF_LONGRUN_SUM: {
                metric_family_t fam = {
                    .name = state->perf_value->metric_longrun[PERF_LONGRUN_SUM],
                    .type = METRIC_TYPE_GAUGE,
                    .help = state->perf_value->description,
                };
                metric_family_append(&fam, VALUE_GAUGE(atof(num)), state->labels, NULL);
                plugin_dispatch_metric_family(&fam, state->time);
            }   break;
            case PERF_LONGRUN_AVGTIME: {
                metric_family_t fam = {
                    .name = state->perf_value->metric_longrun[PERF_LONGRUN_AVGTIME],
                    .type = METRIC_TYPE_GAUGE,
                    .help = state->perf_value->description,
                };
                metric_family_append(&fam, VALUE_GAUGE(atof(num)), state->labels, NULL);
                plugin_dispatch_metric_family(&fam, state->time);
            }   break;
            case PERF_LONGRUN_AVGCOUNT: {
                metric_family_t fam = {
                    .name = state->perf_value->metric_longrun[PERF_LONGRUN_AVGCOUNT],
                    .type = METRIC_TYPE_GAUGE,
                    .help = state->perf_value->description,
                };
                metric_family_append(&fam, VALUE_GAUGE(atof(num)), state->labels, NULL);
                plugin_dispatch_metric_family(&fam, state->time);
            }   break;
            default:
                break;
            }
        }
#if 0
        fprintf(stderr, "[%s] <%d> %s|%s|%s|%s|%s [%s]\n", state->perf_value->metric,
                 state->perf_value->type,
                (state->perf_value->type & PERF_COUNTER_TIME) ?  "tm" : "",
                (state->perf_value->type & PERF_COUNTER_U64)  ?  "u64" : "",
                (state->perf_value->type & PERF_COUNTER_LONGRUNAVG) ? "avg": "",
                (state->perf_value->type & PERF_COUNTER_COUNTER) ? "cnt": "",
                (state->perf_value->type & PERF_COUNTER_HISTOGRAM) ?  "his": "", num);
#endif
        break;
    }

    return true;
}

static bool ceph_data_cb_start_map(void *ctx)
{
    json_struct_t *state = (json_struct_t *)ctx;
    /* Push key to the stack */
    if (state->depth == JSON_MAX_DEPTH)
        return 0;

    state->depth++;

    switch (state->depth) {
    case 1:
        state->perf_key = NULL;
        state->perf_value = NULL;
        state->perf_longrun = PERF_LONGRUN_NONE;
        break;
    case 2:
        state->perf_value = NULL;
        state->perf_longrun = PERF_LONGRUN_NONE;
        break;
    case 3:
        state->perf_longrun = PERF_LONGRUN_NONE;
        break;
    }

    return true;
}

static bool ceph_data_cb_end_map(void *ctx)
{
    json_struct_t *state = (json_struct_t *)ctx;

    /* Pop key from the stack */
    if (state->depth == 0)
        return false;

    state->depth--;

    switch (state->depth) {
    case 1:
        state->perf_key = NULL;
        state->perf_value = NULL;
        state->perf_longrun = PERF_LONGRUN_NONE;
        break;
    case 2:
        state->perf_value = NULL;
        state->perf_longrun = PERF_LONGRUN_NONE;
        break;
    case 3:
        state->perf_longrun = PERF_LONGRUN_NONE;
        break;
    }

    return true;
}

static bool ceph_data_cb_map_key(void *ctx, const char *key, size_t string_len)
{
    json_struct_t *state = (json_struct_t *)ctx;
    char name[512];

    if (string_len > sizeof(name)-1)
        return false;

    switch (state->depth) {
    case 1: {
        perf_key_t *perf_key = NULL;
        memcpy(name, key, string_len);
        name[string_len] = '\0';
        int status = c_avl_get(state->tree, name, (void *)&perf_key);
        if (status == 0)
            state->perf_key = perf_key;
    }   break;
    case 2:
        if (state->perf_key != NULL) {
            perf_value_t *perf_value = NULL;
            memcpy(name, key, string_len);
            name[string_len] = '\0';
            int status = c_avl_get(state->perf_key->tree, name, (void *)&perf_value);
            if (status == 0)
                state->perf_value = perf_value;
        }
        break;
    case 3:
        state->perf_longrun = PERF_LONGRUN_NONE;
        if (state->perf_value != NULL) {
            switch(string_len) {
            case 3:
                if (strncmp("sum", (const char *)key, string_len) == 0)
                    state->perf_longrun = PERF_LONGRUN_SUM;
                break;
            case 7:
                if (strncmp("avgtime", (const char *)key, string_len) == 0)
                    state->perf_longrun = PERF_LONGRUN_AVGTIME;
                break;
            case 8:
                if (strncmp("avgcount", (const char *)key, string_len) == 0)
                    state->perf_longrun = PERF_LONGRUN_AVGCOUNT;
                break;
            }
        }
        break;
    }

    return true;
}

static json_callbacks_t data_callbacks = {
    .json_null        = NULL,
    .json_boolean     = NULL,
    .json_integer     = NULL,
    .json_double      = NULL,
    .json_number      = ceph_data_cb_number,
    .json_string      = NULL,
    .json_start_map   = ceph_data_cb_start_map,
    .json_map_key     = ceph_data_cb_map_key,
    .json_end_map     = ceph_data_cb_end_map,
    .json_start_array = NULL,
    .json_end_array   = NULL,
};

static void ceph_daemon_free(void *arg)
{
    ceph_daemon_t *cd = arg;
    if (cd == NULL)
        return;

    free(cd->name);
    free(cd->asok_path);
    label_set_reset(&cd->labels);

    if (cd->schema!= NULL) {
        while (true) {
            perf_key_t *perf_key = NULL;
            char *key = NULL;
            int status = c_avl_pick(cd->schema, (void *)&key, (void *)&perf_key);
            if (status != 0)
                break;
            ceph_perf_key_free(perf_key);
        }
        c_avl_destroy(cd->schema);
    }

    free(cd);
}

static int ceph_conn_connect(ceph_conn_t *io)
{
    if (io->state != CSTATE_UNCONNECTED) {
        PLUGIN_ERROR("ceph_conn_connect: io->state != CSTATE_UNCONNECTED");
        return -EDOM;
    }

    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        int err = -errno;
        PLUGIN_ERROR("ceph_conn_connect: socket(PF_UNIX, SOCK_STREAM, 0) failed: error %d", err);
        return err;
    }

    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    ssnprintf(address.sun_path, sizeof(address.sun_path), "%s", io->d->asok_path);
    int err;
    RETRY_ON_EINTR(err, connect(fd, (struct sockaddr *)&address,
                                                            sizeof(struct sockaddr_un)));
    if (err < 0) {
        PLUGIN_ERROR("ceph_conn_connect: connect(%d) failed: error %d", fd, err);
        close(fd);
        return err;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        err = -errno;
        PLUGIN_ERROR("ceph_conn_connect: fcntl(%d, O_NONBLOCK) error %d", fd, err);
        close(fd);
        return err;
    }

    io->asok = fd;
    io->state = CSTATE_WRITE_REQUEST;
    io->amt = 0;
    io->json_len = 0;
    io->json = NULL;
    return 0;
}

static void ceph_conn_close(ceph_conn_t *io)
{
    io->state = CSTATE_UNCONNECTED;
    if (io->asok != -1) {
        int res;
        RETRY_ON_EINTR(res, close(io->asok));
    }
    io->asok = -1;
    io->amt = 0;
    io->json_len = 0;
    free(io->json);
    io->json = NULL;
}

/**
 * Initiate JSON parsing and print error if one occurs
 */
static int ceph_conn_process_json(ceph_conn_t *io)
{
    if ((io->request_type != ASOK_REQ_DATA) && (io->request_type != ASOK_REQ_SCHEMA))
        return -EDOM;

    json_callbacks_t *callbacks = io->request_type == ASOK_REQ_SCHEMA ? &schema_callbacks
                                                                      : &data_callbacks;

    json_parser_t handle = {0};
    json_parser_init(&handle, 0, callbacks, (void *)(&io->yajl));

    io->yajl.depth = 0;
    io->yajl.time = cdtime();

    if (io->request_type == ASOK_REQ_DATA) {
        io->yajl.tree = io->d->schema;
        io->yajl.labels = &io->d->labels;
    }

    json_status_t status = json_parser_parse(&handle, io->json, io->json_len);
    switch (status) {
    case JSON_STATUS_OK: {
        unsigned char *errmsg = json_parser_get_error(&handle, 1, io->json,
                                                                  (unsigned int)io->json_len);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free(&handle);
        return 1;
    }   break;
    case JSON_STATUS_CLIENT_CANCELED:
        json_parser_free(&handle);
        return 1;
        break;
    default:
        break;
    }

    status = json_parser_complete(&handle);
    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 0, NULL, 0);
        PLUGIN_ERROR("json_parse_complete failed: %s", (const char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free(&handle);
        return 1;
    }

    if (io->request_type == ASOK_REQ_SCHEMA)
        io->d->schema = io->yajl.tree;

    json_parser_free(&handle);
    return 0;
}

static int ceph_conn_validate_revents(ceph_conn_t *io, int revents)
{
    if (revents & POLLERR) {
        PLUGIN_ERROR("ceph_conn_validate_revents(name=%s): got POLLERR", io->d->name);
        return -EIO;
    }

    switch (io->state) {
    case CSTATE_WRITE_REQUEST:
        return (revents & POLLOUT) ? 0 : -EINVAL;
    case CSTATE_READ_VERSION:
    case CSTATE_READ_AMT:
    case CSTATE_READ_JSON:
        return (revents & POLLIN) ? 0 : -EINVAL;
    default:
        PLUGIN_ERROR("ceph_conn_validate_revents(name=%s) got to illegal state.", io->d->name);
        return -EDOM;
    }
}

/** Handle a network event for a connection */
static ssize_t ceph_conn_handle_event(ceph_conn_t *io)
{
    ssize_t ret = 0;

    switch (io->state) {
    case CSTATE_UNCONNECTED:
        PLUGIN_ERROR("ceph_conn_handle_event(name=%s) got to illegal state.", io->d->name);
        return -EDOM;
    case CSTATE_WRITE_REQUEST: {
        char cmd[32];
        ssnprintf(cmd, sizeof(cmd), "%s%u%s", "{ \"prefix\": \"", io->request_type, "\" }\n");
        size_t cmd_len = strlen(cmd);
        RETRY_ON_EINTR(ret, write(io->asok, ((char *)&cmd) + io->amt, cmd_len - io->amt));
        PLUGIN_DEBUG("ceph_conn_handle_event(name=%s,state=%u,amt=%u,ret=%zd)",
                     io->d->name, io->state, io->amt, ret);
        if (ret < 0)
            return ret;

        io->amt += ret;
        if (io->amt >= cmd_len) {
            io->amt = 0;
            switch (io->request_type) {
            case ASOK_REQ_VERSION:
                io->state = CSTATE_READ_VERSION;
                break;
            default:
                io->state = CSTATE_READ_AMT;
                break;
            }
        }
        return 0;
    }
    case CSTATE_READ_VERSION: {
        RETRY_ON_EINTR(ret, read(io->asok, ((char *)(&io->d->version)) + io->amt,
                                           sizeof(io->d->version) - io->amt));
        PLUGIN_DEBUG("ceph_conn_handle_event(name=%s,state=%u,ret=%zd)",
                     io->d->name, io->state, ret);
        if (ret < 0)
            return ret;

        io->amt += ret;
        if (io->amt >= sizeof(io->d->version)) {
            io->d->version = ntohl(io->d->version);
            if (io->d->version != 1) {
                PLUGIN_ERROR("ceph_conn_handle_event(name=%s) not expecting version %u!",
                             io->d->name, io->d->version);
                return -ENOTSUP;
            }
            PLUGIN_DEBUG("ceph_conn_handle_event(name=%s): identified as version %u",
                         io->d->name, io->d->version);
            io->amt = 0;
            ceph_conn_close(io);
            io->request_type = ASOK_REQ_SCHEMA;
        }
        return 0;
    }
    case CSTATE_READ_AMT: {
        size_t count = sizeof(io->json_len) - io->amt;
        RETRY_ON_EINTR(ret, read(io->asok, ((char *)(&io->json_len)) + io->amt, count));
        PLUGIN_DEBUG("ceph_conn_handle_event(name=%s,state=%u,ret=%zd)",
                     io->d->name, io->state, ret);
        if (ret < 0)
            return ret;
        io->amt += ret;
        if (io->amt >= sizeof(io->json_len)) {
            io->json_len = ntohl(io->json_len);
            io->amt = 0;
            io->state = CSTATE_READ_JSON;
            io->json = calloc(1, io->json_len + 1);
            if (!io->json) {
                PLUGIN_ERROR("error callocing io->json");
                return -ENOMEM;
            }
        }
        return 0;
    }
    case CSTATE_READ_JSON: {
        RETRY_ON_EINTR(ret, read(io->asok, io->json + io->amt, io->json_len - io->amt));
        PLUGIN_DEBUG("ceph_conn_handle_event(name=%s,state=%u,ret=%zd)",
                     io->d->name, io->state, ret);
        if (ret < 0)
            return ret;
        io->amt += ret;
        if (io->amt >= io->json_len) {
            /* coverity[TAINTED_SCALAR] */
            ret = ceph_conn_process_json(io);
            if (ret)
                return ret;
            ceph_conn_close(io);
            io->request_type = ASOK_REQ_NONE;
        }
        return 0;
    }
    default:
        PLUGIN_ERROR("ceph_conn_handle_event(name=%s) got to illegal state.", io->d->name);
        return -EDOM;
    }
}

static int ceph_conn_prepare(ceph_conn_t *io, struct pollfd *fds)
{
    int ret;
    if (io->request_type == ASOK_REQ_NONE) {
        /* The request has already been serviced. */
        return 0;
    } else if ((io->request_type == ASOK_REQ_DATA) && (io->d->schema == NULL)) { // FIXME ¿?
        /* If there are no counters to report on, don't bother connecting */
        return 0;
    }

    switch (io->state) {
    case CSTATE_UNCONNECTED:
        ret = ceph_conn_connect(io);
        if (ret > 0) {
            return -ret;
        } else if (ret < 0) {
            return ret;
        }
        fds->fd = io->asok;
        fds->events = POLLOUT;
        return 1;
    case CSTATE_WRITE_REQUEST:
        fds->fd = io->asok;
        fds->events = POLLOUT;
        return 1;
    case CSTATE_READ_VERSION:
    case CSTATE_READ_AMT:
    case CSTATE_READ_JSON:
        fds->fd = io->asok;
        fds->events = POLLIN;
        return 1;
    default:
         PLUGIN_ERROR("ceph_conn_prepare(name=%s) got to illegal state.", io->d->name);
        return -EDOM;
    }
}

static ssize_t ceph_conn_loop(ceph_daemon_t *cd, uint32_t request_type)
{
    PLUGIN_DEBUG("entering ceph_conn_loop(request_type = %" PRIu32 ")", request_type);
    ceph_conn_t io = {
        .d = cd,
        .request_type = request_type,
        .state = CSTATE_UNCONNECTED,
        .asok = -1,
    };

    cdtime_t end = cdtime() + cd->timeout;

    ssize_t ret = 0;

    while (1) {
        struct pollfd fd = {0};

        ret = ceph_conn_prepare(&io, &fd);
        if (ret < 0) {
            PLUGIN_WARNING("ceph_conn_prepare(name=%s,st=%u)=%zd", io.d->name, io.state, ret);
            ceph_conn_close(&io);
            io.request_type = ASOK_REQ_NONE;
        }
        if (ret != 1) {
            ret = 0;
            goto done;
        }

        cdtime_t now = cdtime();
        if (now > end) {
            ret = -ETIMEDOUT;
            PLUGIN_WARNING("ceph_conn_loop: timed out.");
            goto done;
        }

        cdtime_t diff = end - now;

        RETRY_ON_EINTR(ret, poll(&fd, 1, CDTIME_T_TO_MS(diff)));
        if (ret < 0) {
            PLUGIN_ERROR("poll(2) error: %zd", ret);
            goto done;
        }

        if (fd.revents == 0) {
            /* do nothing */
            continue;
        } else if (ceph_conn_validate_revents(&io, fd.revents)) {
            PLUGIN_WARNING("cconn(name=%s,st=%u): revents validation error: revents=0x%08x",
                            io.d->name, io.state, (unsigned int)fd.revents);
            ceph_conn_close(&io);
            io.request_type = ASOK_REQ_NONE;
        } else {
            ret = ceph_conn_handle_event(&io);
            if (ret) {
                PLUGIN_WARNING("ceph_conn_handle_event(name=%s,st=%u): error %zd",
                                io.d->name, io.state, ret);
                ceph_conn_close(&io);
                io.request_type = ASOK_REQ_NONE;
            }
        }
    }
done:
    ceph_conn_close(&io);

    return ret;
}

static int ceph_read(user_data_t *user_data)
{
    ceph_daemon_t *cd = user_data->data;
    if (cd == NULL)
        return -1;

    if (cd->have_schema == false) {
        int status = (int)ceph_conn_loop(cd, ASOK_REQ_VERSION);
        if (status != 0)
            return -1;
        cd->have_schema = true;
    }

    return (int)ceph_conn_loop(cd, ASOK_REQ_DATA);
}

static int ceph_config_daemon(config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("'daemon' blocks need exactly one string argument.");
        return -1;
    }

    ceph_daemon_t *cd = calloc(1, sizeof(*cd));
    if (cd == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &cd->name);
    if (status != 0) {
        ceph_daemon_free(cd);
        return -1;
    }

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("socket-path", child->key) == 0)
            status = cf_util_get_string(child, &cd->asok_path);
        else if (strcasecmp("label", child->key) == 0)
            status = cf_util_get_label(child, &cd->labels);
        else if (strcasecmp("timeout", child->key) == 0)
            status = cf_util_get_cdtime(child, &cd->timeout);
        else if (strcasecmp("interval", child->key) == 0)
            status = cf_util_get_cdtime(child, &interval);
        else {
            PLUGIN_ERROR("Option `%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        if (cd->asok_path == NULL) {
            PLUGIN_ERROR("%s: you must configure an administrative socket path.\n", cd->name);
            status = -1;
        } else if (!((cd->asok_path[0] == '/') ||
                     (cd->asok_path[0] == '.' && cd->asok_path[1] == '/'))) {
            PLUGIN_ERROR("%s: administrative socket paths must begin "
                         "with '/' or './' Can't parse: '%s'\n", cd->name, cd->asok_path);
            status = -1;
        }
    }

    if (cd->timeout == 0)
        cd->timeout = TIME_T_TO_CDTIME_T(CEPH_TIMEOUT_INTERVAL);

    if (status != 0) {
        ceph_daemon_free(cd);
        return -1;
    }

    label_set_add(&cd->labels, true, "daemon", cd->name);

    return plugin_register_complex_read("ceph", cd->name, ceph_read, interval,
                                        &(user_data_t){.data = cd, .free_func = ceph_daemon_free});
}

static int ceph_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("daemon", child->key) == 0) {
            status = ceph_config_daemon(child);
        } else {
            PLUGIN_WARNING("ignoring unknown option %s", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }
    return 0;
}

static int ceph_init(void)
{
#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_DAC_OVERRIDE)
    if (plugin_check_capability(CAP_DAC_OVERRIDE) != 0) {
        if (getuid() == 0)
            PLUGIN_WARNING("Running ncollectd as root, but the "
                           "CAP_DAC_OVERRIDE capability is missing. The plugin's read "
                           "function will probably fail. Is your init system dropping "
                           "capabilities?");
        else
            PLUGIN_WARNING("ncollectd doesn't have the CAP_DAC_OVERRIDE "
                           "capability. If you don't want to run ncollectd as root, try running "
                           "'setcap cap_dac_override=ep' on the ncollectd binary.");
    }
#endif
    return 0;
}

void module_register(void)
{
    plugin_register_init("ceph", ceph_init);
    plugin_register_config("ceph", ceph_config);
}
