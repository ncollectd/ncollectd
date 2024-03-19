/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"
#include "libutils/strlist.h"
#include "libxson/json_parse.h"
#include "libxson/render.h"
#include "libmdb/table.h"
#include "libmdb/strlist.h"

typedef enum {
    JSON_LIST_NONE,
    JSON_LIST_IN_ARRAY,
} json_list_state_t;

typedef struct {
    json_list_state_t state;
    strlist_t *list;
} json_ctx_t;

static bool handle_string (void *ctx, const char *str, size_t str_len)
{
    json_ctx_t *jctx = ctx;
    if (jctx->state != JSON_LIST_IN_ARRAY)
        return 0;

    strlist_nappend(jctx->list, str, str_len);

    return true;
}

static bool handle_double(__attribute__((unused)) void *ctx,
                          __attribute__((unused)) double double_val)
{
    return false;
}

static bool handle_start_map (__attribute__((unused)) void *ctx)
{
    return false;
}

static bool handle_map_key(__attribute__((unused)) void * ctx,
                           __attribute__((unused)) const char *key,
                           __attribute__((unused)) size_t size)
{
    return false;
}

static bool handle_end_map (__attribute__((unused)) void *ctx)
{
    return true;
}

static bool handle_start_array (void *ctx)
{
    json_ctx_t *jctx = ctx;
    if (jctx->state != JSON_LIST_NONE)
        return false;

    jctx->state = JSON_LIST_IN_ARRAY;
    return true;
}

static bool handle_end_array (void *ctx)
{
    json_ctx_t *jctx = ctx;
    jctx->state = JSON_LIST_NONE;
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

strlist_t *mdb_strlist_parse(const char *data, size_t len)
{
    json_ctx_t jctx = {0};

    strlist_t *list = strlist_alloc(0);
    if (list == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    jctx.list = list;

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
        strlist_free(list);
        return NULL;
    }

    json_parser_free(&handle);

    return list;
}

static int mdb_strlist_render(strlist_t *list, strbuf_t *buf, xson_render_type_t type,
                                                              xson_render_option_t options)
{
    xson_render_t r = {0};

    xson_render_init(&r, buf, type, options);

    xson_render_status_t rstatus = xson_render_array_open(&r);
    for (size_t i = 0; i < list->size; i++) {
        if (list->ptr[i] != NULL)
            rstatus |= xson_render_string(&r, list->ptr[i]);
    }
    rstatus |= xson_render_array_close(&r);

    return rstatus;
}

int mdb_strlist_to_json(strlist_t *list, strbuf_t *buf, bool pretty)
{
    return mdb_strlist_render(list, buf, XSON_RENDER_TYPE_JSON,
                                    pretty ? XSON_RENDER_OPTION_JSON_BEAUTIFY : 0);
}

int mdb_strlist_to_yaml(strlist_t *list, strbuf_t *buf)
{
    return mdb_strlist_render(list, buf, XSON_RENDER_TYPE_SYAML, 0);
}

int mdb_strlist_to_text(strlist_t *list, strbuf_t *buf)
{
    int status = 0;
    for (size_t i = 0; i < list->size; i++) {
        if (list->ptr[i] != NULL) {
            status |= strbuf_putstr(buf, list->ptr[i]);
            status |= strbuf_putchar(buf, '\n');
        }
    }

    return status;
}

int mdb_strlist_to_table(strlist_t *list, table_style_type_t style, strbuf_t *buf, char *header)
{
    size_t max_len[1] = {0};
    size_t header_len = 0;

    if (header != NULL) {
        header_len = strlen(header);
        max_len[0] = header_len;
    }

    for (size_t i = 0; i < list->size; i++) {
        if (list->ptr[i] != NULL) {
            size_t len = strlen(list->ptr[i]);
            if (len > max_len[0])
                max_len[0] = len;
        }
    }

    int status = 0;

    table_t tbl = {0};
    table_init(&tbl, buf, style, max_len, 1, 1);

    status |= table_begin(&tbl);
    status |= table_header_begin(&tbl);
    status |= table_header_cell(&tbl, header);
    status |= table_header_end(&tbl);

    for (size_t i = 0; i < list->size; i++) {
        if (list->ptr[i] != NULL) {
            status |= table_row_begin(&tbl);
            status |= table_row_cell(&tbl, list->ptr[i]);
            status |= table_row_end(&tbl);
        }
    }

    status |= table_table_end(&tbl);

    return status;
}
