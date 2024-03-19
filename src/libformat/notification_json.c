// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009-2015  Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manoj Srivastava <srivasta at google.com>
// SPDX-FileContributor: Elene Margalitadze <elene.margalit at gmail.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libmetric/notification.h"
#include "libutils/common.h"
#include "libxson/render.h"
#include "libxson/json_parse.h"

typedef enum {
    JSON_NOTIF_NONE,
    JSON_NOTIF_IN_MAP,
    JSON_NOTIF_KEY_NAME,
    JSON_NOTIF_KEY_LABELS,
    JSON_NOTIF_KEY_LABELS_KEY,
    JSON_NOTIF_KEY_ANNOTATIONS,
    JSON_NOTIF_KEY_ANNOTATIONS_KEY,
    JSON_NOTIF_KEY_SEVERITY,
    JSON_NOTIF_KEY_TIMESTAMP,
} json_series_state_t;

typedef struct {
    json_series_state_t state;
    notification_t *n;
    char key[1024];
    char value[1024];
} json_ctx_t;

static bool handle_string (void *ctx, const char *str, size_t str_len)
{
    json_ctx_t *jctx = ctx;

    switch (jctx->state) {
    case JSON_NOTIF_KEY_NAME:
        if (jctx->n->name != NULL)
            free(jctx->n->name);
        jctx->n->name = sstrndup(str, str_len);
        jctx->state = JSON_NOTIF_IN_MAP;
        return true;
        break;
    case JSON_NOTIF_KEY_SEVERITY:
        switch(str_len) {
        case 2:
            if (strncasecmp("ok", str, str_len) == 0) {
                jctx->n->severity = NOTIF_OKAY;
                jctx->state = JSON_NOTIF_IN_MAP;
                return true;
            }
            break;
        case 7:
            if (strncasecmp("warning", str, str_len) == 0) {
                jctx->n->severity = NOTIF_WARNING;
                jctx->state = JSON_NOTIF_IN_MAP;
                return true;
            } else if (strncasecmp("failure", str, str_len) == 0) {
                jctx->n->severity = NOTIF_FAILURE;
                jctx->state = JSON_NOTIF_IN_MAP;
                return true;
            }
            break;
        }
        return false;
        break;
    case JSON_NOTIF_KEY_LABELS_KEY:
    case JSON_NOTIF_KEY_ANNOTATIONS_KEY: {
        size_t len = sizeof(jctx->value)-1;
        if (str_len < len)
            len = str_len;
        memcpy(jctx->value, str, len);
        jctx->value[len] = '\0';

        if (jctx->state == JSON_NOTIF_KEY_LABELS_KEY) {
            label_set_add(&jctx->n->label, true, jctx->key, jctx->value);
        } else {
            label_set_add(&jctx->n->annotation, true, jctx->key, jctx->value);
        }

        jctx->key[0] = '\0';
        jctx->value[0] = '\0';
        return true;
    }   break;
    default:
        return false;
    }

    return false;
}

static bool handle_number(void * ctx, const char *num, size_t num_len)
{
    json_ctx_t *jctx = ctx;

    if (jctx->state == JSON_NOTIF_KEY_TIMESTAMP) {
        size_t len = sizeof(jctx->value)-1;
        if (num_len < len)
            len = num_len;
        memcpy(jctx->value, (const char *)num, num_len);
        jctx->value[len] = '\0';

        jctx->n->time = DOUBLE_TO_CDTIME_T(atof(jctx->value));
        jctx->state = JSON_NOTIF_IN_MAP;
        return true;
    }

    return false;
}

static bool handle_start_map (void *ctx)
{
    json_ctx_t *jctx = ctx;

    if (jctx->state == JSON_NOTIF_NONE) {
        jctx->state = JSON_NOTIF_IN_MAP;
        return true;
    } else if (jctx->state == JSON_NOTIF_KEY_LABELS) {
        jctx->state = JSON_NOTIF_KEY_LABELS_KEY;
        return true;
    } else if (jctx->state == JSON_NOTIF_KEY_ANNOTATIONS) {
        jctx->state = JSON_NOTIF_KEY_ANNOTATIONS_KEY;
        return true;
    }

    return false;
}

static bool handle_map_key(void * ctx, const char *key, size_t size)
{
    json_ctx_t *jctx = ctx;

    if (jctx->state == JSON_NOTIF_IN_MAP) {
        switch(size) {
        case 4:
            if (strncasecmp("name", key, size) == 0) {
                jctx->state = JSON_NOTIF_KEY_NAME;
                return true;
            }
            break;
        case 6:
            if (strncasecmp("labels", key, size) == 0) {
                jctx->state = JSON_NOTIF_KEY_LABELS;
                return true;
            }
            break;
        case 8:
            if (strncasecmp("severity", key, size) == 0) {
                jctx->state = JSON_NOTIF_KEY_SEVERITY;
                return true;
            }
            break;
        case 9:
            if (strncasecmp("timestamp", key, size) == 0) {
                jctx->state = JSON_NOTIF_KEY_TIMESTAMP;
                return true;
            }
            break;
        case 11:
            if (strncasecmp("annotations", key, size) == 0) {
                jctx->state = JSON_NOTIF_KEY_ANNOTATIONS;
                return true;
            }
            break;
        }
    } else if ((jctx->state == JSON_NOTIF_KEY_LABELS_KEY) ||
               (jctx->state == JSON_NOTIF_KEY_ANNOTATIONS_KEY)) {
        size_t len = sizeof(jctx->key)-1;
        if (size < len)
            len = size;
        memcpy(jctx->key, (const char *)key, len);
        jctx->key[len] = '\0';
        return true;
    }

    return false;
}

static bool handle_end_map (void *ctx)
{
    json_ctx_t *jctx = ctx;

    if (jctx->state == JSON_NOTIF_KEY_LABELS_KEY) {
        jctx->state = JSON_NOTIF_IN_MAP;
        return true;
    } else if (jctx->state == JSON_NOTIF_KEY_ANNOTATIONS_KEY) {
        jctx->state = JSON_NOTIF_IN_MAP;
        return true;
    } else if (jctx->state == JSON_NOTIF_IN_MAP) {
        jctx->state = JSON_NOTIF_NONE;
        return true;
    }

    return false;
}

static bool handle_start_array (__attribute__((unused)) void *ctx)
{
    return false;
}

static bool handle_end_array (__attribute__((unused)) void *ctx)
{
    return false;
}

static bool handle_boolean (__attribute__((unused)) void *ctx, __attribute__((unused)) int value)
{
    return false;
}

static bool handle_null (__attribute__((unused)) void *ctx)
{
    return false;
}

static const json_callbacks_t callbacks = {
    .json_null        = handle_null,        /* null        = */
    .json_boolean     = handle_boolean,     /* boolean     = */
    .json_integer     = NULL,               /* integer     = */
    .json_double      = NULL,               /* double      = */
    .json_number      = handle_number,      /* number      = */
    .json_string      = handle_string,      /* string      = */
    .json_start_map   = handle_start_map,   /* start map   = */
    .json_map_key     = handle_map_key,     /* map key     = */
    .json_end_map     = handle_end_map,     /* end map     = */
    .json_start_array = handle_start_array, /* start array = */
    .json_end_array   = handle_end_array    /* end array   = */
};

notification_t *notification_json_parse(const char *data, size_t len)
{
    json_ctx_t jctx = {0};

    notification_t *n = calloc (1, sizeof(*n));
    if (n == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    jctx.n = n;

    json_parser_t handle;
    json_parser_init(&handle, 0, &callbacks, &jctx);

    json_status_t status = json_parser_parse(&handle, (const unsigned char *) data, len);
    if (status == JSON_STATUS_OK)
        status = json_parser_complete(&handle);

    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 1,
                                                      (const unsigned char *)data , len);
        ERROR("%s", (char *)errmsg);
        notification_free(n);
        json_parser_free_error(errmsg);
        json_parser_free (&handle);
        return NULL;
    }

    json_parser_free(&handle);

    return jctx.n;
}

int notification_json(strbuf_t *buf, notification_t const *n)
{
    xson_render_t r = {0};
    xson_render_init(&r, buf, XSON_RENDER_TYPE_JSON, 0);

    int status = xson_render_map_open(&r);

    status |= xson_render_key_string(&r, "name");
    status |= xson_render_string(&r, n->name);

    status |= xson_render_key_string(&r, "labels");
    status |= xson_render_map_open(&r);
    for (size_t i = 0; i < n->label.num; i++) {
        status |= xson_render_key_string(&r, n->label.ptr[i].name);
        status |= xson_render_string(&r, n->label.ptr[i].value);
    }
    status |= xson_render_map_close(&r);

    status |= xson_render_key_string(&r, "annotations");
    status |= xson_render_map_open(&r);
    for (size_t i = 0; i < n->annotation.num; i++) {
        status |= xson_render_key_string(&r, n->annotation.ptr[i].name);
        status |= xson_render_string(&r, n->annotation.ptr[i].value);
    }
    status |= xson_render_map_close(&r);

    status |= xson_render_key_string(&r, "severity");
    switch (n->severity) {
    case NOTIF_FAILURE:
        status |= xson_render_string(&r, "failure");
        break;
    case NOTIF_WARNING:
        status |= xson_render_string(&r, "warning");
        break;
    case NOTIF_OKAY:
        status |= xson_render_string(&r, "ok");
        break;
    default:
        status |= xson_render_string(&r, "warning");
        break;
    }

    status |= xson_render_key_string(&r, "timestamp");
    status |= xson_render_integer(&r, CDTIME_T_TO_DOUBLE(n->time));

    status |= xson_render_map_close(&r);

    return status;
}
