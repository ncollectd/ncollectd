// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/common.h"
#include "libxson/json_parse.h"
#include "libxson/render.h"
#include "libmdb/series_list.h"

typedef enum {
    JSON_SERIES_NONE,
    JSON_SERIES_IN_ARRAY,
    JSON_SERIES_IN_MAP,
    JSON_SERIES_MAP_KEY_NAME,
    JSON_SERIES_MAP_KEY_LABEL
} json_series_state_t;

typedef struct {
    json_series_state_t state;
    struct {
        size_t alloc;
        size_t size;
        mdb_series_t *ptr;
    } list;
    char key[1024];
    char value[1024];
} json_ctx_t;

static bool handle_string (void *ctx, const char *str, size_t str_len)
{
    json_ctx_t *jctx = ctx;

    if (jctx->state == JSON_SERIES_MAP_KEY_NAME) {
        char *nstr = sstrndup(str, str_len);
        if (nstr == NULL) {
            ERROR("Out of memory");
            return false;
        }
        jctx->list.ptr[jctx->list.size-1].name = nstr;
        jctx->state = JSON_SERIES_IN_MAP;
        return true;
    } else if (jctx->state == JSON_SERIES_MAP_KEY_LABEL) {
        size_t len = sizeof(jctx->value)-1;
        if (str_len < len)
            len = str_len;
        memcpy(jctx->value, str, len);
        jctx->value[len] = '\0';

        label_set_add(&jctx->list.ptr[jctx->list.size-1].labels, true, jctx->key, jctx->value);

        jctx->key[0] = '\0';
        jctx->value[0] = '\0';
        jctx->state = JSON_SERIES_IN_MAP;
        return true;
    }

    return false;
}

static bool handle_double(__attribute__((unused)) void *ctx, __attribute__((unused)) double double_val)
{
    return false;
}

static bool handle_start_map (void *ctx)
{
    json_ctx_t *jctx = ctx;
    if (jctx->state != JSON_SERIES_IN_ARRAY)
        return false;
    jctx->state = JSON_SERIES_IN_MAP;

    if ((jctx->list.size+1) >= jctx->list.alloc) {
        size_t alloc = jctx->list.alloc == 0 ? 4 : jctx->list.alloc*2;
        mdb_series_t *tmp = realloc(jctx->list.ptr, sizeof(jctx->list.ptr[0])*alloc);
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
    if (jctx->state != JSON_SERIES_IN_MAP)
        return false;

    if ((size == 8) && !strncmp("__name__", key, 8)) {
        jctx->state = JSON_SERIES_MAP_KEY_NAME;
    } else {
        jctx->state = JSON_SERIES_MAP_KEY_LABEL;
        size_t len = sizeof(jctx->key)-1;
        if (size < len)
            len = size;
        memcpy(jctx->key, key, len);
        jctx->key[len] = '\0';
    }

    return true;
}

static bool handle_end_map (void *ctx)
{
    json_ctx_t *jctx = ctx;
    jctx->state = JSON_SERIES_IN_ARRAY;
    return true;
}

static bool handle_start_array (void *ctx)
{
    json_ctx_t *jctx = ctx;
    if (jctx->state != JSON_SERIES_NONE)
        return false;
    jctx->state = JSON_SERIES_IN_ARRAY;
    return true;
}

static bool handle_end_array (void *ctx)
{
    json_ctx_t *jctx = ctx;
    jctx->state = JSON_SERIES_NONE;
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

void mdb_series_list_free(mdb_series_list_t *list)
{
    if (list == NULL)
        return;

    for (size_t i = 0; i < list->num; i++) {
        free(list->ptr[i].name);
        label_set_reset(&list->ptr[i].labels);
    }
    free(list->ptr);
    free(list);
}

mdb_series_list_t *mdb_series_list_parse(const char *data, size_t len)
{
    json_ctx_t jctx = {0};

    mdb_series_list_t *list = calloc(1, sizeof(*list));
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
        mdb_series_list_free(list);
        return NULL;
    }

    json_parser_free(&handle);

    list->num = jctx.list.size;
    list->ptr = jctx.list.ptr;

    return list;
}

int mdb_series_list_render(mdb_series_list_t *list, strbuf_t *buf,
                           xson_render_type_t type, xson_render_option_t options)
{
    xson_render_t r = {0};

    xson_render_init(&r, buf, type, options);

    xson_render_status_t rstatus = xson_render_array_open(&r);
    for (size_t i = 0; i < list->num; i++) {
        rstatus |= xson_render_map_open(&r);
        if (list->ptr[i].name != NULL) {
            rstatus |= xson_render_key_string(&r, "__name__");
            rstatus |= xson_render_string(&r, list->ptr[i].name);
        }
        for (size_t j = 0; j < list->ptr[i].labels.num ; j++) {
            rstatus |= xson_render_key_string(&r, list->ptr[i].labels.ptr[j].name);
            rstatus |= xson_render_string(&r, list->ptr[i].labels.ptr[j].value);
        }
        rstatus |= xson_render_map_close(&r);
    }
    rstatus = xson_render_array_close(&r);

    return rstatus;
}

int mdb_series_list_to_json(mdb_series_list_t *list, strbuf_t *buf, bool pretty)
{
    return mdb_series_list_render(list, buf, XSON_RENDER_TYPE_JSON,
                                        pretty ? XSON_RENDER_OPTION_JSON_BEAUTIFY : 0);
}

int mdb_series_list_to_yaml(mdb_series_list_t *list, strbuf_t *buf)
{
    return mdb_series_list_render(list, buf, XSON_RENDER_TYPE_SYAML, 0);
}

int mdb_series_list_to_text(mdb_series_list_t *list, strbuf_t *buf)
{
    for (size_t i = 0; i < list->num; i++) {
        if (list->ptr[i].name != NULL)
            strbuf_print(buf, list->ptr[i].name);
        if (list->ptr[i].labels.num > 0) {
           strbuf_putchar(buf, '{');
           for (size_t j = 0; j < list->ptr[i].labels.num ; j++) {
               if (j != 0)
                   strbuf_putchar(buf, ',');
               strbuf_putstr(buf, list->ptr[i].labels.ptr[j].name);
               strbuf_putstr(buf, "=\"");
               strbuf_putescape_label(buf, list->ptr[i].labels.ptr[j].value);
               strbuf_putchar(buf, '"');
           }
           strbuf_putchar(buf, '}');
        }
        strbuf_putchar(buf, '\n');
    }
    return 0;
}

int mdb_series_list_to_table(mdb_series_list_t *list, table_style_type_t style, strbuf_t *buf)
{
    size_t max_len[2];
    max_len[0] = strlen("NAME");
    max_len[1] = strlen("LABELS");

    for (size_t i = 0; i < list->num; i++) {
        size_t len = 0;
        if (list->ptr[i].name != NULL) {
            len = strlen(list->ptr[i].name);
            if (len > max_len[0])
                max_len[0] = len;
        }

        len = label_set_strlen(&list->ptr[i].labels);
        if (len > max_len[1])
            max_len[1] = len;
    }

    int status = 0;

    table_t tbl = {0};
    table_init(&tbl, buf, style, max_len, 2, 1);

    status |= table_begin(&tbl);
    status |= table_header_begin(&tbl);
    status |= table_header_cell(&tbl, "NAME");
    status |= table_header_cell(&tbl, "LABELS");
    status |= table_header_end(&tbl);

    char buffer[4096];
    strbuf_t lbuf = STRBUF_CREATE_STATIC(buffer);

    for (size_t i = 0; i < list->num; i++) {
        status |= table_row_begin(&tbl);
        status |= table_row_cell(&tbl, list->ptr[i].name);


        for (size_t j = 0; j < list->ptr[i].labels.num ; j++) {
            if (j != 0)
                strbuf_putchar(&lbuf, ',');
            strbuf_putstr(&lbuf, list->ptr[i].labels.ptr[j].name);
            strbuf_putstr(&lbuf, "=\"");
            strbuf_putescape_label(&lbuf, list->ptr[i].labels.ptr[j].value);
            strbuf_putchar(&lbuf, '"');
        }
        status |= table_row_cell(&tbl, lbuf.ptr);
        strbuf_reset(&lbuf);

        status |= table_row_end(&tbl);
    }

    status |= table_table_end(&tbl);

    return status;
}

