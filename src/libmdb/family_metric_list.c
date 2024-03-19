// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"
#include "libxson/json_parse.h"
#include "libxson/render.h"
#include "libmdb/family_metric_list.h"

typedef enum {
    JSON_METRICS_NONE,
    JSON_METRICS_IN_ARRAY,
    JSON_METRICS_IN_MAP,
    JSON_METRICS_MAP_KEY_NAME,
    JSON_METRICS_MAP_KEY_HELP,
    JSON_METRICS_MAP_KEY_TYPE,
    JSON_METRICS_MAP_KEY_UNIT
} json_family_metrics_state_t;

typedef struct {
    json_family_metrics_state_t state;
    struct {
        size_t alloc;
        size_t size;
        mdb_family_metric_t *ptr;
    } list;
} json_ctx_t;

static bool handle_string (void *ctx, const char *str, size_t str_len)
{
    json_ctx_t *jctx = ctx;

    if (jctx->state == JSON_METRICS_MAP_KEY_NAME) {
        jctx->list.ptr[jctx->list.size-1].name = sstrndup((const char *)str, str_len);
        if (jctx->list.ptr[jctx->list.size-1].name == NULL) {
            ERROR("Out of memory");
            return false;
        }
        jctx->state = JSON_METRICS_IN_MAP;
        return true;
    } else if (jctx->state == JSON_METRICS_MAP_KEY_HELP) {
        jctx->list.ptr[jctx->list.size-1].help = sstrndup((const char *)str, str_len);
        if (jctx->list.ptr[jctx->list.size-1].help == NULL) {
            ERROR("Out of memory");
            return false;
        }
        jctx->state = JSON_METRICS_IN_MAP;
        return true;
    } else if (jctx->state == JSON_METRICS_MAP_KEY_TYPE) {
        switch(str_len) {
        case 4:
            if (strncmp("info", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_INFO;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            }
            break;
        case 5:
            if (strncmp("gauge", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_GAUGE;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            }
            break;
        case 7:
            if (strncmp("summary", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_SUMMARY;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            } else if (strncmp("unknown", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_UNKNOWN;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            } else if (strncmp("counter", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_COUNTER;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            }
            break;
        case 8:
            if (strncmp("stateset", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_STATE_SET;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            }
            break;
        case 9:
            if (strncmp("histogram", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_HISTOGRAM;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            }
            break;
        case 14:
            if (strncmp("gaugehistogram", (const char *)str, str_len) == 0) {
                jctx->list.ptr[jctx->list.size-1].type = METRIC_TYPE_GAUGE_HISTOGRAM;
                jctx->state = JSON_METRICS_IN_MAP;
                return true;
            }
            break;
        }
        return false;
    } else if (jctx->state == JSON_METRICS_MAP_KEY_UNIT) {
        jctx->list.ptr[jctx->list.size-1].unit= sstrndup((const char *)str, str_len);
        if (jctx->list.ptr[jctx->list.size-1].unit== NULL) {
            ERROR("Out of memory");
            return false;
        }
        jctx->state = JSON_METRICS_IN_MAP;
        return true;
    }

    return false;
}

static bool handle_double(__attribute__((unused)) void *ctx,
                         __attribute__((unused)) double double_val)
{
    return false;
}

static bool handle_start_map (void *ctx)
{
    json_ctx_t *jctx = ctx;
    if (jctx->state != JSON_METRICS_IN_ARRAY)
        return false;
    jctx->state = JSON_METRICS_IN_MAP;

    if ((jctx->list.size+1) >= jctx->list.alloc) {
        size_t alloc = jctx->list.alloc == 0 ? 4 : jctx->list.alloc*2;
        mdb_family_metric_t *tmp = realloc(jctx->list.ptr, sizeof(jctx->list.ptr[0])*alloc);
        if (tmp == NULL) {
            ERROR("Out of memory");
            return false;
        }
        jctx->list.ptr = tmp;
        jctx->list.alloc = alloc;
    }
    memset(jctx->list.ptr+jctx->list.size, 0, sizeof(jctx->list.ptr[0]));
    jctx->list.size++;

    return true;
}

static bool handle_map_key(void * ctx, const char *key, size_t size)
{
    json_ctx_t *jctx = ctx;

    if (size != 4)
        return false;

    switch(key[0]) {
    case 'n':
        if ((key[1] == 'a') && (key[2] == 'm') && (key[3] == 'e')) {
            jctx->state = JSON_METRICS_MAP_KEY_NAME;
            return true;
        }
        break;
    case 'h':
        if ((key[1] == 'e') && (key[2] == 'l') && (key[3] == 'p')) {
            jctx->state = JSON_METRICS_MAP_KEY_HELP;
            return true;
        }
        break;
    case 'u':
        if ((key[1] == 'n') && (key[2] == 'i') && (key[3] == 't')) {
            jctx->state = JSON_METRICS_MAP_KEY_UNIT;
            return true;
        }
        break;
    case 't':
        if ((key[1] == 'y') && (key[2] == 'p') && (key[3] == 'e')) {
            jctx->state = JSON_METRICS_MAP_KEY_TYPE;
            return true;
        }
        break;
    }

    return false;
}

static bool handle_end_map (void *ctx)
{
    json_ctx_t *jctx = ctx;
    jctx->state = JSON_METRICS_IN_ARRAY;
    return true;
}

static bool handle_start_array (void *ctx)
{
    json_ctx_t *jctx = ctx;
    if (jctx->state != JSON_METRICS_NONE)
        return false;
    jctx->state = JSON_METRICS_IN_ARRAY;
    return true;
}

static bool handle_end_array (void *ctx)
{
    json_ctx_t *jctx = ctx;
    jctx->state = JSON_METRICS_NONE;
    return true;
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
    .json_double      = handle_double,      /* double      = */
    .json_number      = NULL,               /* number      = */
    .json_string      = handle_string,      /* string      = */
    .json_start_map   = handle_start_map,   /* start map   = */
    .json_map_key     = handle_map_key,     /* map key     = */
    .json_end_map     = handle_end_map,     /* end map     = */
    .json_start_array = handle_start_array, /* start array = */
    .json_end_array   = handle_end_array    /* end array   = */
};

void mdb_family_metric_list_free(mdb_family_metric_list_t *list)
{
    if (list == NULL)
        return;

    if (list->ptr != NULL) {
        for (size_t i = 0; i < list->num; i++) {
            free(list->ptr[i].name);
            free(list->ptr[i].unit);
            free(list->ptr[i].help);
        }
        free (list->ptr);
    }

    free (list);
}

int mdb_family_metric_list_append(mdb_family_metric_list_t *list, char *name, metric_type_t type,
                                                          char *unit, char *help)
{
    if (name == NULL)
        return 0;

    char *nname = sstrdup(name);
    if (nname == NULL)
        return -1;

    char *nunit = NULL;
    if (unit != NULL) {
        nunit = sstrdup(unit);
        if (nunit == NULL) {
            free(nname);
            return -1;
        }
    }

    char *nhelp = NULL;
    if (help != NULL) {
        nhelp = sstrdup(help);
        if (nhelp == NULL) {
            free(nname);
            free(nunit);
            return -1;
        }
    }

    mdb_family_metric_t *ptr = realloc(list->ptr, sizeof(list->ptr[0])*(list->num + 1));
    if (ptr == NULL) {
        free(nname);
        free(nunit);
        free(nhelp);
        return -1;
    }

    list->ptr = ptr;
    list->ptr[list->num].name = nname;
    list->ptr[list->num].type = type;
    list->ptr[list->num].unit = nunit;
    list->ptr[list->num].help = nhelp;
    list->num++;

    return 0;
}

mdb_family_metric_list_t *mdb_family_metric_list_parse(const char *data, size_t len)
{
    json_ctx_t jctx = {0};

    mdb_family_metric_list_t *list = calloc(1, sizeof(*list));
    if (list == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    json_parser_t handle;
    json_parser_init(&handle, 0, &callbacks, &jctx);

    json_status_t status = json_parser_parse(&handle, (const unsigned char *) data, len);
    if (status == JSON_STATUS_OK)
        status = json_parser_complete(&handle);

    if (status != JSON_STATUS_OK) {
        unsigned char *errmsg = json_parser_get_error(&handle, 1,
                                                      (const unsigned char *)data , len);
        ERROR("%s", (char *)errmsg);
        json_parser_free_error(errmsg);
        json_parser_free (&handle);
        mdb_family_metric_list_free(list);
        return NULL;
    }

    json_parser_free(&handle);

    list->num = jctx.list.size;
    list->ptr = jctx.list.ptr;

    return list;
}
static int mdb_family_metric_list_render(mdb_family_metric_list_t *list, strbuf_t *buf,
                                         xson_render_type_t type, xson_render_option_t options)
{
    xson_render_t r = {0};

    xson_render_init(&r, buf, type, options);

    xson_render_status_t rstatus = xson_render_array_open(&r);
    for (size_t i = 0; i < list->num; i++) {
        if (list->ptr[i].name != NULL) {
            rstatus |= xson_render_map_open(&r);
            rstatus |= xson_render_key_string(&r, "name");
            rstatus |= xson_render_string(&r, list->ptr[i].name);
            if (list->ptr[i].help != NULL) {
                rstatus |= xson_render_key_string(&r, "help");
                rstatus |= xson_render_string(&r, list->ptr[i].help);
            }
            if (list->ptr[i].unit != NULL) {
                rstatus |= xson_render_key_string(&r, "unit");
                rstatus |= xson_render_string(&r, list->ptr[i].unit);
            }
            rstatus |= xson_render_key_string(&r, "type");
            rstatus |= xson_render_string(&r, metric_type_str(list->ptr[i].type));
            rstatus |= xson_render_map_close(&r);
        }
    }
    rstatus |= xson_render_array_close(&r);
    return rstatus;
}

int mdb_family_metric_list_to_json(mdb_family_metric_list_t *list, strbuf_t *buf, bool pretty)
{
    return mdb_family_metric_list_render(list, buf, XSON_RENDER_TYPE_JSON,
                                               pretty ? XSON_RENDER_OPTION_JSON_BEAUTIFY : 0);
}

int mdb_family_metric_list_to_yaml(mdb_family_metric_list_t *list, strbuf_t *buf)
{
    return mdb_family_metric_list_render(list, buf, XSON_RENDER_TYPE_SYAML, 0);
}

int mdb_family_metric_list_to_text(mdb_family_metric_list_t *list, strbuf_t *buf)
{
    for (size_t i = 0; i < list->num; i++) {
        if (list->ptr[i].name != NULL) {
            strbuf_putstrn(buf, "# TYPE ", strlen("# TYPE "));
            strbuf_putstr(buf, metric_type_str(list->ptr[i].type));
            strbuf_putchar(buf, ' ');
            strbuf_putstr(buf, list->ptr[i].name);
            strbuf_putchar(buf, '\n');

            if (list->ptr[i].unit != NULL) {
                strbuf_putstrn(buf, "# UNIT ", strlen("# UNIT "));
                strbuf_putstr(buf, list->ptr[i].name);
                strbuf_putchar(buf, ' ');
                strbuf_putstr(buf, list->ptr[i].unit);
                strbuf_putchar(buf, '\n');
            }
            if (list->ptr[i].help != NULL) {
                strbuf_putstrn(buf, "# HELP ", strlen("# HELP "));
                strbuf_putstr(buf, list->ptr[i].name);
                strbuf_putchar(buf, ' ');
                strbuf_putstr(buf, list->ptr[i].help);
                strbuf_putchar(buf, '\n');
            }
            strbuf_putchar(buf, '\n');
        }
    }

    return 0;
}

int mdb_family_metric_list_to_table(mdb_family_metric_list_t *list, table_style_type_t style, strbuf_t *buf)
{
    size_t max_len[4];
    max_len[0] = strlen("NAME");
    max_len[1] = strlen("TYPE");
    max_len[2] = strlen("UNIT");
    max_len[3] = strlen("HELP");

    for (size_t i = 0; i < list->num; i++) {
        if (list->ptr[i].name != NULL) {
            size_t len = strlen(list->ptr[i].name);
            if (len > max_len[0])
                max_len[0] = len;
        }

        {
            size_t len = strlen(metric_type_str(list->ptr[i].type));
            if (len > max_len[1])
                max_len[1] = len;
        }

        if (list->ptr[i].unit != NULL) {
            size_t len = strlen(list->ptr[i].unit);
            if (len > max_len[2])
                max_len[2] = len;
        }

        if (list->ptr[i].help != NULL) {
            size_t len = strlen(list->ptr[i].help);
            if (len > max_len[3])
                max_len[3] = len;
        }
    }

    int status = 0;

    table_t tbl = {0};
    table_init(&tbl, buf, style, max_len, 4, 1);

    status |= table_begin(&tbl);
    status |= table_header_begin(&tbl);
    status |= table_header_cell(&tbl, "NAME");
    status |= table_header_cell(&tbl, "TYPE");
    status |= table_header_cell(&tbl, "UNIT");
    status |= table_header_cell(&tbl, "HELP");
    status |= table_header_end(&tbl);

    for (size_t i = 0; i < list->num; i++) {
        status |= table_row_begin(&tbl);
        status |= table_row_cell(&tbl, list->ptr[i].name);
        status |= table_row_cell(&tbl, metric_type_str(list->ptr[i].type));
        status |= table_row_cell(&tbl, list->ptr[i].unit);
        status |= table_row_cell(&tbl, list->ptr[i].help);
        status |= table_row_end(&tbl);
    }

    status |= table_table_end(&tbl);

    return status;
}

