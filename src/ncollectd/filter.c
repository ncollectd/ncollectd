// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "configfile.h"
#include "libutils/common.h"
#include "libutils/complain.h"
#include "libutils/config.h"
#include "libmetric/metric_chars.h"
#include "plugin_internal.h"
#include "filter.h"

#include <regex.h>

typedef enum {
    FILTER_STMT_TYPE_IF,
    FILTER_STMT_TYPE_ELIF,
    FILTER_STMT_TYPE_ELSE,
    FILTER_STMT_TYPE_UNLESS,
    FILTER_STMT_TYPE_DROP,
    FILTER_STMT_TYPE_STOP,
    FILTER_STMT_TYPE_RETURN,
    FILTER_STMT_TYPE_CALL,
    FILTER_STMT_TYPE_WRITE,
    FILTER_STMT_TYPE_METRIC_RENAME,
    FILTER_STMT_TYPE_LABEL_SET,
    FILTER_STMT_TYPE_LABEL_UNSET,
    FILTER_STMT_TYPE_LABEL_RENAME,
    FILTER_STMT_TYPE_LABEL_ALLOW,
    FILTER_STMT_TYPE_LABEL_IGNORE,
    FILTER_STMT_TYPE_METRIC_SUB,
    FILTER_STMT_TYPE_METRIC_GSUB,
    FILTER_STMT_TYPE_LABEL_SUB,
    FILTER_STMT_TYPE_LABEL_GSUB,
    FILTER_STMT_TYPE_LABEL_VALUE_SUB,
    FILTER_STMT_TYPE_LABEL_VALUE_GSUB,
    FILTER_STMT_TYPE_METRIC_MATCH,
    FILTER_STMT_TYPE_LABEL_VALUE_MATCH,
} filter_stmt_type_t;

typedef enum {
    FILTER_SUB_STR,
    FILTER_SUB_REF,
    FILTER_SUB_NAME,
    FILTER_SUB_LABEL,
} filter_sub_type_t;

typedef struct {
    filter_sub_type_t type;
    union {
        size_t ref;
        char *str;
    };
} filter_sub_t;

typedef struct {
    size_t num;
    filter_sub_t *ptr;
} filter_sub_list_t;

typedef struct {
    c_complain_t complaint;
    char *plugin;
} filter_stmt_write_plugin_t;

typedef enum {
    FILTER_LIST_TYPE_STR,
    FILTER_LIST_TYPE_REGEX,
} filter_list_type_t;

typedef struct {
    filter_list_type_t type;
    union {
        char *str;
        regex_t *regex;
    };
} filter_list_item_t;

typedef struct {
    size_t num;
    filter_list_item_t *ptr;
} filter_stmt_list_t;

struct filter_stmt_s {
    filter_stmt_type_t type;
    union {
        struct {
            metric_match_t match;
            filter_stmt_t *stmt;
            filter_stmt_t *next;
        } stmt_if;
        struct {
            filter_t *filter;
        } stmt_call;
        struct {
            c_complain_t complaint;
            char *plugin;
        } stmt_write_plugin;
        struct {
            c_complain_t complaint;
            size_t num;
            filter_stmt_write_plugin_t *ptr;
        } stmt_write;
        struct {
            filter_sub_list_t to;
        } stmt_metric_rename;
        struct {
            char *label;
            filter_sub_list_t value;
        } stmt_label_set;
        struct {
            char *label;
        } stmt_label_unset;
        struct {
            char *from;
            filter_sub_list_t to;
        } stmt_label_rename;
        struct {
            char *label;
            regex_t *regex;
            filter_sub_list_t replace;
        } stmt_sub;
        struct {
            char *label;
            regex_t *regex;
            filter_stmt_t *stmt;
        } stmt_match;
        filter_stmt_list_t stmt_list;
    };
    filter_stmt_t *next;
};

struct filter_s {
    char *name;
    filter_stmt_t *ptr;
};

typedef struct {
    size_t size;
    filter_t **ptr;
} filter_list_t;

typedef enum {
    FILTER_FAM_METRIC_ALLOC = (1 <<  0),
} filter_flags_t;

static filter_list_t filter_global_list;

static filter_stmt_t *filter_config_stmt(const config_item_t *ci, bool *error, bool global);

void filter_stmt_free(filter_stmt_t *root);

static filter_stmt_t *filter_stmt_alloc(filter_stmt_type_t type)
{
    filter_stmt_t *stmt = calloc (1, sizeof(*stmt));
    if (stmt == NULL) {
        ERROR("Calloc failed.\n");
        return NULL;
    }

    stmt->type = type;
    return stmt;
}

static filter_t *filter_alloc(char *name)
{
    filter_t *filter = calloc(1, sizeof(*filter));
    if (filter == NULL) {
        ERROR("calloc failed.");
        return NULL;
    }

    if (name != NULL) {
        filter->name = strdup(name);
        if (filter->name == NULL) {
            ERROR("strdup failed.");
            free(filter);
            return NULL;
        }
    }

    return filter;
}

void filter_free(filter_t *filter)
{
    if (filter == NULL)
        return;

    free(filter->name);
    filter_stmt_free(filter->ptr);
    free(filter);
}

void plugin_filter_free(filter_t *filter)
{
    if (filter == NULL)
        return;

    free(filter->name);
    filter_stmt_free(filter->ptr);
    free(filter);
}

void filter_reset(filter_t *filter)
{
    if (filter == NULL)
        return;

    free(filter->name);
    filter->name = NULL;
    filter_stmt_free(filter->ptr);
    filter->ptr = NULL;
}

void filter_global_free(void)
{
    for (size_t i=0; i < filter_global_list.size; i++) {
        filter_free(filter_global_list.ptr[i]);
    }
    free(filter_global_list.ptr);
    filter_global_list.ptr = NULL;
    filter_global_list.size = 0;
}

filter_t *filter_global_get_by_name(const char *name)
{
    if (name == NULL)
        return NULL;

    for (size_t i=0; i < filter_global_list.size; i++) {
        filter_t *filter = filter_global_list.ptr[i];
        if ((filter != NULL) && (filter->name != NULL)) {
            if (strcmp(name, filter->name) == 0)
                return filter;
        }
    }

    return NULL;
}

static int filter_global_list_append(filter_t *filter)
{
    filter_t **tmp = realloc(filter_global_list.ptr,
                             sizeof(filter_global_list.ptr[0])*(filter_global_list.size + 1));
    if (tmp == NULL) {
        ERROR("realloc failed.");
        return -1;
    }

    filter_global_list.ptr = tmp;
    filter_global_list.ptr[filter_global_list.size] = filter;
    filter_global_list.size++;

    return 0;
}

static void filter_sub_list_reset(filter_sub_list_t *slist)
{
    if (slist == NULL)
        return;

    if (slist->ptr != NULL) {
        for (size_t i = 0; i < slist->num; i++) {
            if ((slist->ptr[i].type == FILTER_SUB_STR) ||
                (slist->ptr[i].type == FILTER_SUB_LABEL))
                free(slist->ptr[i].str);
        }
        free(slist->ptr);
    }

    slist->ptr = NULL;
    slist->num = 0;
}

static filter_sub_t *filter_sub_list_append(filter_sub_list_t *slist)
{
    filter_sub_t *tmp = realloc(slist->ptr, sizeof(slist->ptr[0]) * (slist->num + 1));
    if (tmp == NULL) {
        ERROR("realloc failed.");
        return NULL;
    }

    slist->ptr = tmp;
    memset(&slist->ptr[slist->num], 0, sizeof(slist->ptr[slist->num]));
    slist->num++;

    return &slist->ptr[slist->num-1];
}

static int filter_sub_add_label(filter_sub_list_t *slist, char *str, size_t len)
{
    char *new_str = NULL;
    if (!((len == strlen("__name__")) && (strncmp(str, "__name__", len) == 0))) {
        new_str = strndup(str, len);
        if (new_str == NULL) {
            ERROR("strndup failed.");
            return -1;
        }
    }

    filter_sub_t *sub = filter_sub_list_append(slist);
    if (sub == NULL) {
        free(new_str);
        return -1;
    }

    if (new_str != NULL) {
        sub->type = FILTER_SUB_LABEL;
        sub->str = new_str;
    } else {
        sub->type = FILTER_SUB_NAME;
    }

    return 0;
}

static int filter_sub_add_str(filter_sub_list_t *slist, char *str, size_t len)
{
    char *new_str = strndup(str, len);
    if (new_str == NULL) {
        ERROR("strndup failed.");
        return -1;
    }

    filter_sub_t *sub = filter_sub_list_append(slist);
    if (sub == NULL) {
        free(new_str);
        return -1;
    }

    sub->type = FILTER_SUB_STR;
    sub->str = new_str;
    return 0;
}

static int filter_sub_add_ref(filter_sub_list_t *slist, int ref)
{
    filter_sub_t *sub = filter_sub_list_append(slist);
    if (sub == NULL)
        return -1;

    sub->type = FILTER_SUB_REF;
    sub->ref = ref;
    return 0;
}

static int filter_sub_parse(filter_sub_list_t *slist, char *str)
{
    if (str == NULL)
        return 0;

    size_t len = strlen(str);
    if (len == 0)
        return 0;

    strbuf_t buf = STRBUF_CREATE;
    int status = strbuf_resize(&buf, len);

    char *c = str;
    while (*c != '\0') {
        if (status != 0)
            PLUGIN_WARNING("Failed to fill buffer.");

        if (*c == '$') {
            if (strbuf_len(&buf) > 0) {
                filter_sub_add_str(slist, buf.ptr, strbuf_len(&buf));
                strbuf_reset(&buf);
            }

            c++;

            if (*c == '\0') {
                status |= strbuf_putchar(&buf, '$');
                continue;
            }

            if (*c == '$') {
                status |= strbuf_putchar(&buf, *c);
                c++;
                continue;
            }

            if ((*c >= '0') && (*c <= '9')) {
                filter_sub_add_ref(slist, *c - '0');
                c++;
                continue;
            }

            if (*c == '{') {
                c++;
                char *name = c;
                size_t name_len = 0;
                while (*c != '\0') {
                    if (*c == '}') {
                        c++;
                        break;
                    }
                    c++;
                    name_len++;
                }
                if (name_len > 0) {
                    if ((name_len == 1) && (*name >= '0') && (*name <= '9')) {
                        filter_sub_add_ref(slist, *name - '0');
                    } else {
                        filter_sub_add_label(slist, name, name_len);
                    }
                    continue;
                }

            }

            if (valid_label_chars[(unsigned char)*c] == 1) {
                char *name = c;
                c++;
                size_t name_len = 1;
                while (*c != '\0') {
                    if (valid_label_chars[(unsigned char)*c] == 0)
                        break;
                    c++;
                    name_len++;
                }
                filter_sub_add_label(slist, name, name_len);
                continue;
            }
        }

        if (*c != '\0') {
            status |= strbuf_putchar(&buf, *c);
            c++;
        }
    }

    if (strbuf_len(&buf) > 0)
        filter_sub_add_str(slist, buf.ptr, strbuf_len(&buf));

    strbuf_destroy(&buf);
    return 0;
}

static filter_stmt_t *filter_config_stmt_if(const config_item_t *ci, filter_stmt_type_t type,
                                                                     bool global)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        ERROR("'%s' statement require exactly one string argument in %s:%d.",
              ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(type);
    if (stmt == NULL)
        return NULL;

    int status = metric_match_unmarshal(&stmt->stmt_if.match, ci->values[0].value.string);
    if (status != 0) {
        ERROR("Fail to parse match: '%s' in '%s' statement in %s:%d.",
              ci->values[0].value.string, ci->key, cf_get_file(ci), cf_get_lineno(ci));
        filter_stmt_free(stmt);
        return NULL;
    }

    bool error = false;
    stmt->stmt_if.stmt = filter_config_stmt(ci, &error, global);
    if ((stmt->stmt_if.stmt == NULL) && error) {
        filter_stmt_free(stmt);
        return NULL;
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_else(const config_item_t *ci, bool global)
{
    if (ci->values_num != 0){
        ERROR("'else' statement does not have arguments in %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_ELSE);
    if (stmt == NULL)
        return NULL;

    bool error = false;
    stmt->stmt_if.stmt = filter_config_stmt(ci, &error, global);
    if ((stmt->stmt_if.stmt == NULL) && error) {
        filter_stmt_free(stmt);
        return NULL;
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_call(const config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        ERROR("'call' statement require exactly one string argument in %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_t *filter = filter_global_get_by_name(ci->values[0].value.string);
    if (filter == NULL) {
        ERROR("Filter '%s' not found in %s:%d.", ci->values[0].value.string,
              cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_CALL);
    if (stmt == NULL)
        return NULL;

    stmt->stmt_call.filter = filter;

    return stmt;
}

static filter_stmt_t *filter_config_stmt_write(const config_item_t *ci)
{
    if (ci->values_num > 1) {
        for (int i = 0; i < ci->values_num; i++) {
            if (ci->values[0].type != CONFIG_TYPE_STRING) {
                ERROR("'write' statement require a list of strings as argument in %s:%d.",
                      cf_get_file(ci), cf_get_lineno(ci));
                return NULL;
            }
        }
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_WRITE);
    if (stmt == NULL) {
        return NULL;
    }

    if (ci->values_num > 1) {
        stmt->stmt_write.ptr = malloc(sizeof(*(stmt->stmt_write.ptr)) * ci->values_num);
        if (stmt->stmt_write.ptr == NULL) {
            ERROR("malloc failed.");
            filter_stmt_free(stmt);
            return NULL;
        }

        for (int i = 0; i < ci->values_num; i++) {
            stmt->stmt_write.ptr[i].plugin = strdup(ci->values[i].value.string);
            C_COMPLAIN_INIT(&stmt->stmt_write.ptr[i].complaint);
        }

        stmt->stmt_write.num = ci->values_num;
    } else {
        C_COMPLAIN_INIT(&stmt->stmt_write.complaint);
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_metric_rename(const config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        ERROR("'metric-rename' statement require exactly one string argument in %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_METRIC_RENAME);
    if (stmt == NULL)
        return NULL;

    int status = filter_sub_parse(&stmt->stmt_metric_rename.to, ci->values[0].value.string);
    if (status != 0) {
        ERROR("Failed to parse '%s' in %s:%d.",
              ci->values[0].value.string, cf_get_file(ci), cf_get_lineno(ci));
        filter_stmt_free(stmt);
        return NULL;
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_label_set(const config_item_t *ci)
{
    if ((ci->values_num != 2) || (ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        WARNING("'label-set' statement require exactly two string argument in %s:%d.",
                cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_LABEL_SET);
    if (stmt == NULL)
        return NULL;

    stmt->stmt_label_set.label = strdup(ci->values[0].value.string);
    if (stmt->stmt_label_set.label == NULL) {
        ERROR("strdup failed.");
        filter_stmt_free(stmt);
        return NULL;
    }

    int status = filter_sub_parse(&stmt->stmt_label_set.value, ci->values[1].value.string);
    if (status != 0) {
        ERROR("Failed to parse '%s' in %s:%d.",
              ci->values[1].value.string, cf_get_file(ci), cf_get_lineno(ci));
        filter_stmt_free(stmt);
        return NULL;
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_label_unset(const config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        ERROR("'label-unset' statement require exactly one string argument in %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    if (strcmp(ci->values[0].value.string, "__name__") == 0) {
        ERROR("Cannot unset metric name in %s:%d.", cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    char *label = strdup(ci->values[0].value.string);
    if (label == NULL) {
        ERROR("strdup failed.");
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_LABEL_UNSET);
    if (stmt == NULL) {
        free(label);
        return NULL;
    }

    stmt->stmt_label_unset.label = label;

    return stmt;
}

static filter_stmt_t *filter_config_stmt_label_rename(const config_item_t *ci)
{
    if ((ci->values_num != 2) || (ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        ERROR("'label-rename' statement require exactly two string argument %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(FILTER_STMT_TYPE_LABEL_RENAME);
    if (stmt == NULL)
        return NULL;

    stmt->stmt_label_rename.from = strdup(ci->values[0].value.string);
    if (stmt->stmt_label_rename.from == NULL) {
        ERROR("strdup failed.");
        filter_stmt_free(stmt);
        return NULL;
    }

    int status = filter_sub_parse(&stmt->stmt_label_rename.to, ci->values[1].value.string);
    if (status != 0) {
        ERROR("Failed to parse '%s' in %s:%d.",
              ci->values[1].value.string, cf_get_file(ci), cf_get_lineno(ci));
        filter_stmt_free(stmt);
        return NULL;
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_list(const config_item_t *ci, filter_stmt_type_t type)
{
    if (ci->values_num < 1) {
        ERROR("'%s' statement require a list of strings as argument in %s:%d.",
              ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return NULL;
    }

    for (int i = 0; i < ci->values_num; i++) {
        if ((ci->values[0].type != CONFIG_TYPE_STRING) &&
            (ci->values[0].type != CONFIG_TYPE_REGEX)) {
            ERROR("'%s' statement require a list of strings or regex as argument in %s:%d.",
                  ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return NULL;
        }
    }

    filter_stmt_t *stmt = filter_stmt_alloc(type);
    if (stmt == NULL)
        return NULL;

    stmt->stmt_list.ptr = calloc(ci->values_num, sizeof(*(stmt->stmt_list.ptr)));
    if (stmt->stmt_list.ptr == NULL) {
        ERROR("malloc failed.");
        filter_stmt_free(stmt);
        return NULL;
    }
    stmt->stmt_list.num = ci->values_num;

    for (int i = 0; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            stmt->stmt_list.ptr[i].type = FILTER_LIST_TYPE_STR;
            stmt->stmt_list.ptr[i].str = strdup(ci->values[i].value.string);
        } else if (ci->values[i].type != CONFIG_TYPE_REGEX) {
            stmt->stmt_list.ptr[i].type = FILTER_LIST_TYPE_REGEX;
            stmt->stmt_list.ptr[i].regex = calloc(1, sizeof(*stmt->stmt_list.ptr[i].regex));
            if (stmt->stmt_list.ptr[i].regex == NULL) {
                ERROR("calloc failed.");
                filter_stmt_free(stmt);
                return NULL;
            }

            int status = regcomp(stmt->stmt_list.ptr[i].regex, ci->values[i].value.string,
                                                               REG_EXTENDED);
            if (status != 0) {
                ERROR("regcom '%s' failed in %s:%d: %s.", ci->values[i].value.string,
                      cf_get_file(ci), cf_get_lineno(ci), STRERRNO);
                filter_stmt_free(stmt);
                return NULL;
            }
        }
    }


    return stmt;
}

static filter_stmt_t *filter_config_stmt_sub(const config_item_t *ci, filter_stmt_type_t type)
{
    char *label = NULL;
    char *pattern = NULL;
    char *replace = NULL;

    if((type == FILTER_STMT_TYPE_METRIC_SUB) || (type == FILTER_STMT_TYPE_METRIC_GSUB) ||
       (type == FILTER_STMT_TYPE_LABEL_SUB) || (type == FILTER_STMT_TYPE_LABEL_GSUB)) {

        if ((ci->values_num != 2) || (ci->values[0].type != CONFIG_TYPE_REGEX) ||
                                     (ci->values[1].type != CONFIG_TYPE_STRING)) {
            ERROR("'%s' statement require exactly two arguments: regex and string in %s:%d.",
                  ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return NULL;
        }

        pattern = ci->values[0].value.string;
        replace = ci->values[1].value.string;
    } else if ((type == FILTER_STMT_TYPE_LABEL_VALUE_SUB) ||
               (type == FILTER_STMT_TYPE_LABEL_VALUE_GSUB)) {

        if ((ci->values_num != 3) || (ci->values[0].type != CONFIG_TYPE_STRING) ||
                                     (ci->values[1].type != CONFIG_TYPE_REGEX) ||
                                     (ci->values[2].type != CONFIG_TYPE_STRING)) {
            ERROR("'%s' statement require exactly tree arguments: string, regex and string "
                  "in %s:%d.", ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return NULL;
        }

        label = ci->values[0].value.string;
        pattern = ci->values[1].value.string;
        replace = ci->values[2].value.string;
    } else {
        return NULL;
    }

    filter_stmt_t *stmt = filter_stmt_alloc(type);
    if (stmt == NULL)
        return NULL;

    if (label != NULL)  {
        stmt->stmt_sub.label = strdup(label);
        if (stmt->stmt_sub.label == NULL) {
            filter_stmt_free(stmt);
            return NULL;
        }
    }

    if (replace != NULL) {
        int status = filter_sub_parse(&stmt->stmt_sub.replace, replace);
        if (status != 0) {
            ERROR("Failed to parse '%s' in %s:%d.", replace, cf_get_file(ci), cf_get_lineno(ci));
            filter_stmt_free(stmt);
            return NULL;
        }
    }

    stmt->stmt_sub.regex = calloc(1, sizeof(*stmt->stmt_sub.regex));
    if (stmt->stmt_sub.regex == NULL) {
        ERROR("calloc failed.");
        filter_stmt_free(stmt);
        return NULL;
    }

    int status = regcomp(stmt->stmt_sub.regex, pattern, REG_EXTENDED);
    if (status != 0) {
        ERROR("regcom '%s' failed in %s:%d: %s.", pattern, cf_get_file(ci), cf_get_lineno(ci),
              STRERRNO);
        filter_stmt_free(stmt);
        return NULL;
    }

    return stmt;
}

static filter_stmt_t *filter_config_stmt_match(const config_item_t *ci, filter_stmt_type_t type)
{
    char *label = NULL;
    char *pattern = NULL;

    if (type == FILTER_STMT_TYPE_METRIC_MATCH) {
        if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_REGEX)) {
            ERROR("'%s' statement require exactly one regex in %s:%d.",
                  ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return NULL;
        }

        pattern = ci->values[0].value.string;
    } else if (type == FILTER_STMT_TYPE_LABEL_VALUE_MATCH) {
        if ((ci->values_num != 2) || (ci->values[0].type != CONFIG_TYPE_STRING) ||
                                     (ci->values[1].type != CONFIG_TYPE_REGEX)) {
            ERROR("'%s' statement require exactly two arguments: string and regex in %s:%d.",
                  ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return NULL;
        }

        label = ci->values[0].value.string;
        pattern = ci->values[1].value.string;
    } else {
        return NULL;
    }

    filter_stmt_t *match_stmt = filter_stmt_alloc(type);
    if (match_stmt == NULL)
        return NULL;

    if (label != NULL)  {
        match_stmt->stmt_match.label = strdup(label);
        if (match_stmt->stmt_match.label == NULL) {
            filter_stmt_free(match_stmt);
            return NULL;
        }
    }

    match_stmt->stmt_match.regex = calloc(1, sizeof(*match_stmt->stmt_match.regex));
    if (match_stmt->stmt_match.regex == NULL) {
        filter_stmt_free(match_stmt);
        return NULL;
    }

    int status = regcomp(match_stmt->stmt_match.regex, pattern, REG_EXTENDED);
    if (status != 0) {
        ERROR("regcom '%s' failed in %s:%d: %s.", pattern,
              cf_get_file(ci), cf_get_lineno(ci), STRERRNO);
        filter_stmt_free(match_stmt);
        return NULL;
    }

    filter_stmt_t *root_stmt = NULL;
    filter_stmt_t *prev_stmt = NULL;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *cstmt = ci->children + i;
        filter_stmt_t *stmt = NULL;

        if (strcasecmp("metric-rename", cstmt->key) == 0) {
            stmt = filter_config_stmt_metric_rename(cstmt);
        } else if (strcasecmp("label-set", cstmt->key) == 0) {
            stmt = filter_config_stmt_label_set(cstmt);
        } else if (strcasecmp("label-unset", cstmt->key) == 0) {
            stmt = filter_config_stmt_label_unset(cstmt);
        } else if (strcasecmp("label-rename", cstmt->key) == 0) {
            stmt = filter_config_stmt_label_rename(cstmt);
        } else {
            ERROR("Option '%s' in filter match %s:%d is not allowed.",
                    cstmt->key, cf_get_file(cstmt), cf_get_lineno(cstmt));
            break;
        }

        if (stmt == NULL) {
            filter_stmt_free(root_stmt);
            filter_stmt_free(match_stmt);
            return NULL;
        }

        if (root_stmt == NULL)
            root_stmt = stmt;
        if (prev_stmt != NULL)
            prev_stmt->next = stmt;
        prev_stmt = stmt;
    }

    match_stmt->stmt_match.stmt = root_stmt;

    return match_stmt;
}

void filter_stmt_free(filter_stmt_t *root)
{
    while (root != NULL) {
        filter_stmt_t *next = root->next;

        switch(root->type) {
        case FILTER_STMT_TYPE_IF:
        case FILTER_STMT_TYPE_UNLESS:
        case FILTER_STMT_TYPE_ELIF: {
            metric_match_reset(&root->stmt_if.match);
            if (root->stmt_if.stmt != NULL)
                filter_stmt_free(root->stmt_if.stmt);
            if (root->stmt_if.next != NULL)
                filter_stmt_free(root->stmt_if.next);
        }   break;
        case FILTER_STMT_TYPE_ELSE:
            if (root->stmt_if.stmt != NULL)
                filter_stmt_free(root->stmt_if.stmt);
            break;
        case FILTER_STMT_TYPE_DROP:
            break;
        case FILTER_STMT_TYPE_STOP:
            break;
        case FILTER_STMT_TYPE_RETURN:
            break;
        case FILTER_STMT_TYPE_CALL:
            break;
        case FILTER_STMT_TYPE_WRITE:
            if (root->stmt_write.ptr != NULL) {
                for (size_t i=0; i < root->stmt_write.num; i++) {
                    free(root->stmt_write.ptr[i].plugin);
                }
                free(root->stmt_write.ptr);
            }
            break;
        case FILTER_STMT_TYPE_METRIC_RENAME:
            filter_sub_list_reset(&root->stmt_metric_rename.to);
            break;
        case FILTER_STMT_TYPE_LABEL_SET:
            free(root->stmt_label_set.label);
            filter_sub_list_reset(&root->stmt_label_set.value);
            break;
        case FILTER_STMT_TYPE_LABEL_UNSET:
            free(root->stmt_label_unset.label);
            break;
        case FILTER_STMT_TYPE_LABEL_RENAME:
            free(root->stmt_label_rename.from);
            filter_sub_list_reset(&root->stmt_label_rename.to);
            break;
        case FILTER_STMT_TYPE_LABEL_ALLOW:
        case FILTER_STMT_TYPE_LABEL_IGNORE:
            if (root->stmt_list.ptr != NULL) {
                for (size_t i=0; i < root->stmt_list.num; i++) {
                    if (root->stmt_list.ptr[i].type == FILTER_LIST_TYPE_STR) {
                        free(root->stmt_list.ptr[i].str);
                    } else if (root->stmt_list.ptr[i].type == FILTER_LIST_TYPE_REGEX) {
                        regfree(root->stmt_list.ptr[i].regex);
                        free(root->stmt_list.ptr[i].regex);
                    }
                }
                free(root->stmt_list.ptr);
            }
            break;
        case FILTER_STMT_TYPE_METRIC_SUB:
        case FILTER_STMT_TYPE_METRIC_GSUB:
        case FILTER_STMT_TYPE_LABEL_SUB:
        case FILTER_STMT_TYPE_LABEL_GSUB:
        case FILTER_STMT_TYPE_LABEL_VALUE_SUB:
        case FILTER_STMT_TYPE_LABEL_VALUE_GSUB:
            free(root->stmt_sub.label);
            if (root->stmt_sub.regex != NULL) {
                regfree(root->stmt_sub.regex);
                free(root->stmt_sub.regex);
            }
            filter_sub_list_reset(&root->stmt_sub.replace);
            break;
        case FILTER_STMT_TYPE_METRIC_MATCH:
        case FILTER_STMT_TYPE_LABEL_VALUE_MATCH:
            free(root->stmt_match.label);
            if (root->stmt_match.regex != NULL) {
                regfree(root->stmt_match.regex);
                free(root->stmt_match.regex);
            }
            filter_stmt_free(root->stmt_match.stmt);
            break;
        }

        free(root);

        root = next;
    }
}

static filter_stmt_t *filter_config_stmt(const config_item_t *ci, bool *error, bool global)
{
    filter_stmt_t *root_stmt = NULL;
    filter_stmt_t *prev_stmt = NULL;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *cstmt = ci->children + i;
        filter_stmt_t *stmt = NULL;

        if (strcasecmp("if", cstmt->key) == 0) {
            stmt = filter_config_stmt_if(cstmt, FILTER_STMT_TYPE_IF, global);
        } else if (strcasecmp("unless", cstmt->key) == 0) {
            stmt = filter_config_stmt_if(cstmt, FILTER_STMT_TYPE_UNLESS, global);
        } else if (strcasecmp("elif", cstmt->key) == 0) {
            if ((prev_stmt == NULL) ||
                    ((prev_stmt->type != FILTER_STMT_TYPE_IF) &&
                     (prev_stmt->type != FILTER_STMT_TYPE_UNLESS))) {
                ERROR("'elif' block without previus 'if', 'unless' or 'elif' block in %s:%d.",
                       cf_get_file(cstmt), cf_get_lineno(cstmt));
                filter_stmt_free(root_stmt);
                return NULL;
            }

            filter_stmt_t *last = prev_stmt->stmt_if.next;
            if (last != NULL) {
                while (last->next != NULL)
                    last = last->next;
                if (last->type != FILTER_STMT_TYPE_ELIF) {
                    ERROR("'elif' block without previus 'if', 'unless' or 'elif' block in %s:%d.",
                          cf_get_file(cstmt), cf_get_lineno(cstmt));
                    filter_stmt_free(root_stmt);
                    return NULL;
                }
            }

            stmt = filter_config_stmt_if(cstmt, FILTER_STMT_TYPE_ELIF, global);
            if (stmt != NULL) {
                if (last == NULL) {
                    prev_stmt->stmt_if.next = stmt;
                } else {
                    last->next = stmt;
                }
                continue;
            }
        } else if (strcasecmp("else", cstmt->key) == 0) {
            if ((prev_stmt == NULL) ||
                    ((prev_stmt->type != FILTER_STMT_TYPE_IF) &&
                     (prev_stmt->type != FILTER_STMT_TYPE_UNLESS))) {
                ERROR("'else' block without previus 'if', 'unless' or 'elif' block in %s:%d.",
                      cf_get_file(cstmt), cf_get_lineno(cstmt));
                filter_stmt_free(root_stmt);
                return NULL;
            }

            filter_stmt_t *last = prev_stmt->stmt_if.next;
            if (last != NULL) {
                while (last->next != NULL)
                    last = last->next;
                if (last->type != FILTER_STMT_TYPE_ELIF) {
                    ERROR("'else' block without previus 'if', 'unless' or 'elif' block in %s:%d.",
                          cf_get_file(cstmt), cf_get_lineno(cstmt));
                    filter_stmt_free(root_stmt);
                    return NULL;
                }
            }

            stmt = filter_config_stmt_else(cstmt, global);
            if (stmt != NULL) {
                if (last == NULL) {
                    prev_stmt->stmt_if.next = stmt;
                } else {
                    last->next = stmt;
                }
                continue;
            }
        } else if (strcasecmp("drop", cstmt->key) == 0) {
            stmt = filter_stmt_alloc(FILTER_STMT_TYPE_DROP);
        } else if (strcasecmp("stop", cstmt->key) == 0) {
            stmt = filter_stmt_alloc(FILTER_STMT_TYPE_STOP);
        } else if (strcasecmp("return", cstmt->key) == 0) {
            stmt = filter_stmt_alloc(FILTER_STMT_TYPE_RETURN);
        } else if (strcasecmp("call", cstmt->key) == 0) {
            if (global) {
                stmt = filter_config_stmt_call(cstmt);
            } else {
                ERROR("Error 'call' statement in local filter in %s:%d.",
                      cf_get_file(cstmt), cf_get_lineno(cstmt));
                filter_stmt_free(root_stmt);
                return NULL;
            }
        } else if (strcasecmp("write", cstmt->key) == 0) {
            if (global) {
                stmt = filter_config_stmt_write(cstmt);
            } else {
                ERROR("Error 'write' statement in local filter in %s:%d.",
                      cf_get_file(cstmt), cf_get_lineno(cstmt));
                filter_stmt_free(root_stmt);
                return NULL;
            }
        } else if (strcasecmp("metric-rename", cstmt->key) == 0) {
            stmt = filter_config_stmt_metric_rename(cstmt);
        } else if (strcasecmp("label-set", cstmt->key) == 0) {
            stmt = filter_config_stmt_label_set(cstmt);
        } else if (strcasecmp("label-unset", cstmt->key) == 0) {
            stmt = filter_config_stmt_label_unset(cstmt);
        } else if (strcasecmp("label-rename", cstmt->key) == 0) {
            stmt = filter_config_stmt_label_rename(cstmt);
        } else if (strcasecmp("label-allow", cstmt->key) == 0) {
            stmt = filter_config_stmt_list(cstmt, FILTER_STMT_TYPE_LABEL_ALLOW);
        } else if (strcasecmp("label-ignore", cstmt->key) == 0) {
            stmt = filter_config_stmt_list(cstmt, FILTER_STMT_TYPE_LABEL_IGNORE);
        } else if (strcasecmp("metric-sub", cstmt->key) == 0) {
            stmt = filter_config_stmt_sub(cstmt, FILTER_STMT_TYPE_METRIC_SUB);
        } else if (strcasecmp("metric-gsub", cstmt->key) == 0) {
            stmt = filter_config_stmt_sub(cstmt, FILTER_STMT_TYPE_METRIC_GSUB);
        } else if (strcasecmp("label-sub", cstmt->key) == 0) {
            stmt = filter_config_stmt_sub(cstmt, FILTER_STMT_TYPE_LABEL_SUB);
        } else if (strcasecmp("label-gsub", cstmt->key) == 0) {
            stmt = filter_config_stmt_sub(cstmt, FILTER_STMT_TYPE_LABEL_GSUB);
        } else if (strcasecmp("label-value-sub", cstmt->key) == 0) {
            stmt = filter_config_stmt_sub(cstmt, FILTER_STMT_TYPE_LABEL_VALUE_SUB);
        } else if (strcasecmp("label-value-gsub", cstmt->key) == 0) {
            stmt = filter_config_stmt_sub(cstmt, FILTER_STMT_TYPE_LABEL_VALUE_GSUB);
        } else if (strcasecmp("metric-match", cstmt->key) == 0) {
            stmt = filter_config_stmt_match(cstmt, FILTER_STMT_TYPE_METRIC_MATCH);
        } else if (strcasecmp("label-value-match", cstmt->key) == 0) {
            stmt = filter_config_stmt_match(cstmt, FILTER_STMT_TYPE_LABEL_VALUE_MATCH);
        } else {
            ERROR("Option '%s' in filter %s:%d is not allowed.",
                    cstmt->key, cf_get_file(cstmt), cf_get_lineno(cstmt));
            break;
        }

        if (stmt == NULL) {
            *error = true;
            filter_stmt_free(root_stmt);
            return NULL;
        }

        if (root_stmt == NULL)
            root_stmt = stmt;
        if (prev_stmt != NULL)
            prev_stmt->next = stmt;
        prev_stmt = stmt;
    }

    return root_stmt;
}

int filter_global_configure(const config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        ERROR("Global 'filter' blocks require exactly one string argument in %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    filter_t *filter = filter_global_get_by_name(ci->values[0].value.string);
    if (filter == NULL) {
        filter = filter_alloc(ci->values[0].value.string);
        if (filter == NULL)
            return -1;

        int status = filter_global_list_append(filter);
        if (status < 0) {
            filter_free(filter);
            return -1;
        }
    }

    bool error = false;
    filter_stmt_t *stmt = filter_config_stmt(ci, &error, true);
    if (stmt == NULL) {
        if (error)
            return -1;
        return 0;
    }

    if (filter->ptr == NULL) {
        filter->ptr = stmt;
    } else {
        filter_stmt_t *nstmt = filter->ptr;
        while (nstmt->next != NULL) {
            nstmt = nstmt->next;
        }
        nstmt->next = stmt;
    }

    return 0;
}

int plugin_filter_configure(const config_item_t *ci, filter_t **filter)
{
    if (ci->values_num != 0) {
        ERROR("Local 'filter' cannot have arguments in %s:%d.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (*filter == NULL) {
       *filter = filter_alloc(NULL);
        if (*filter == NULL)
            return -1;
    }

    bool error = false;
    filter_stmt_t *stmt = filter_config_stmt(ci, &error, false);
    if ((stmt == NULL) && error) {
        filter_free(*filter);
        return -1;
    }

    if ((*filter)->ptr == NULL) {
        (*filter)->ptr = stmt;
    } else {
        filter_stmt_t *nstmt = (*filter)->ptr;
        while (nstmt->next != NULL) {
            nstmt = nstmt->next;
        }
        nstmt->next = stmt;
    }

    return 0;
}

static void filter_write(filter_stmt_t *stmt, metric_family_t *fam, metric_t *m)
{
    metric_family_t write_fam = {
        .name = fam->name,
        .help = fam->help,
        .unit = fam->unit,
        .type = fam->type,
        .metric.ptr = m,
        .metric.num = 1,
    };

    if (stmt->stmt_write.num == 0) {
        int status = plugin_write(NULL, &write_fam, true);
        if (status == ENOENT) {
            /* in most cases this is a permanent error, so use the complain
             * mechanism rather than spamming the logs */
            c_complain(LOG_INFO, &stmt->stmt_write.complaint,
                       "Filter 'write': Dispatching value to "
                       "all write plugins failed with status %i (ENOENT). "
                       "Most likely this means you didn't load any write plugins.",
                       status);

            //plugin_log_available_writers();
        } else if (status != 0) {
            /* often, this is a permanent error (e.g. target system unavailable),
             * so use the complain mechanism rather than spamming the logs */
            c_complain(LOG_INFO, &stmt->stmt_write.complaint,
                       "Filter 'write': Dispatching value to "
                       "all write plugins failed with status %i.",
                       status);
        } else {
            assert(status == 0);
            c_release(LOG_INFO, &stmt->stmt_write.complaint,
                                "Filter 'write': Some write plugin is back to normal "
                                "operation. 'write' succeeded.");
        }
    } else {
        for (size_t i = 0; i < stmt->stmt_write.num; i++) {
            int status = plugin_write(stmt->stmt_write.ptr[i].plugin, &write_fam, true);
            if (status != 0) {
                c_complain(LOG_INFO, &stmt->stmt_write.ptr[i].complaint,
                           "Filter 'write': Dispatching value to "
                           "the '%s' plugin failed with status %i.",
                            stmt->stmt_write.ptr[i].plugin, status);

                //plugin_log_available_writers();
            } else {
                c_release(LOG_INFO, &stmt->stmt_write.ptr[i].complaint,
                          "Filter 'write': Plugin '%s' is back "
                          "to normal operation. 'write' succeeded.",
                          stmt->stmt_write.ptr[i].plugin);
            }
        }
    }
}

static int filter_replace(filter_sub_list_t *replace, strbuf_t *buf,
                          regmatch_t *re_match, size_t re_match_num,
                          char *str, char str_len,
                          char *name, label_set_t *labels)
{
    for (size_t i = 0; i < replace->num; i++) {
        filter_sub_t *sub = &replace->ptr[i];
        if (sub->type == FILTER_SUB_STR) {
            strbuf_putstr(buf, sub->str);
        } else if ((sub->type == FILTER_SUB_REF) && (str != NULL)) {
            size_t ref = sub->ref;
            if (ref < re_match_num) {
                if ((re_match[ref].rm_so < 0) || (re_match[ref].rm_eo < 0) ||
                    (re_match[ref].rm_so > re_match[ref].rm_eo) ||
                    (re_match[ref].rm_eo > str_len + 1))
                    return -1;
                size_t match_len = re_match[ref].rm_eo - re_match[ref].rm_so;
                if (match_len > 0)
                    strbuf_putstrn(buf, str + re_match[ref].rm_so, match_len);
            }
        } else if (sub->type == FILTER_SUB_NAME) {
            if (name != NULL)
                strbuf_putstr(buf, name);
        } else if (sub->type == FILTER_SUB_LABEL) {
            if (labels != NULL) {
                label_pair_t *pair = label_set_read(*labels, sub->str);
                if (pair != NULL)
                    strbuf_putstr(buf, pair->value);
            }
        }
    }

    return 0;
}

static int filter_sub(regex_t *reg, strbuf_t *buf, char *str, filter_sub_list_t *replace,
                                    char *name, label_set_t *labels, bool global)
{
    regoff_t str_len = strlen(str);
    int eflags = 0;
    regmatch_t re_match[10];
    int nmatch = 0;

    while(regexec(reg, str, STATIC_ARRAY_SIZE(re_match), re_match, eflags) == 0) {
        regoff_t so = re_match[0].rm_so;
        regoff_t eo = re_match[0].rm_eo;

        if ((so < 0) || (eo < 0) || (so > eo) || (eo > str_len + 1))
            return -1;

        nmatch++;

        strbuf_putstrn(buf, str, so);

        filter_replace(replace, buf, re_match, STATIC_ARRAY_SIZE(re_match), str, str_len,
                                     name, labels);

        str += so;
        str_len -= so;

        if (eo == so) {
            strbuf_putstrn(buf, str, 1);
            str++;
            str_len--;
        } else {
            str += eo - so;
            str_len -= eo - so;
        }

        if (*str == '\0')
            break;

        eflags = REG_NOTBOL;

        if (!global)
            break;
    }

    if (str_len > 0)
        strbuf_putstrn(buf, str, str_len);

    return nmatch;
}

static int filter_list_labels(filter_stmt_list_t *list, filter_stmt_type_t type,
                                                        label_set_t *labels)
{
    size_t remain = 0;

    for (size_t i = 0; i < labels->num; i++) {
        bool remove = type == FILTER_STMT_TYPE_LABEL_ALLOW ? true : false;
        for (size_t j = 0; j < list->num; j++) {
            filter_list_item_t *item = &list->ptr[i];
            if (item->type == FILTER_LIST_TYPE_STR) {
                if (strcmp(labels->ptr[i].name, item->str) == 0) {
                    remove = type == FILTER_STMT_TYPE_LABEL_ALLOW ? false : true;
                    break;
                }
            } else if (item->type == FILTER_LIST_TYPE_REGEX) {
                if (regexec(item->regex, labels->ptr[i].name, 0, NULL, REG_NOSUB) == 0) {
                    remove = type == FILTER_STMT_TYPE_LABEL_ALLOW ? false : true;
                    break;
                }
            }
        }

        if (remove) {
            free(labels->ptr[i].name);
            labels->ptr[i].name = NULL;
            free(labels->ptr[i].value);
            labels->ptr[i].value = NULL;
        } else {
            remain++;
        }
    }

    if (remain == labels->num)
        return 0;

    if (remain == 0) {
        free(labels->ptr);
        labels->ptr = NULL;
        labels->num = 0;
        return 0;
    }

    label_pair_t *tmp = malloc(sizeof(*tmp) * remain);
    if (tmp == NULL) {
        ERROR("malloc failed.");
        label_set_reset(labels);
        return ENOMEM;
    }

    size_t j = 0;
    for (size_t i = 0; i < labels->num; i++) {
        if (labels->ptr[i].name != NULL) {
            tmp[j].name = labels->ptr[i].name;
            tmp[j].value = labels->ptr[i].value;
            j++;
        }
    }

    free(labels->ptr);
    labels->ptr = tmp;
    labels->num = remain;

    return 0;
}

static int filter_stmt_metric_rename(filter_stmt_t *stmt, regmatch_t *re_match, size_t re_match_num,
                                     char *str, char str_len,
                                     uint64_t *flags, metric_family_t *fam, metric_t *m)
{
    char *metric_name = NULL;
    if ((stmt->stmt_metric_rename.to.num == 1) &&
        (stmt->stmt_metric_rename.to.ptr[0].type == FILTER_SUB_STR)) {
        metric_name = strdup(stmt->stmt_metric_rename.to.ptr[0].str);
    } else {
        strbuf_t buf = STRBUF_CREATE;
        filter_replace(&stmt->stmt_metric_rename.to, &buf, re_match, re_match_num, str, str_len,
                                                           fam->name, &m->label);
        if (buf.ptr != NULL)
            metric_name = strdup(buf.ptr);
        strbuf_destroy(&buf);
    }

    if (metric_name != NULL) {
        if (*flags & FILTER_FAM_METRIC_ALLOC)
            free(fam->name);
        fam->name = metric_name;
        *flags |= FILTER_FAM_METRIC_ALLOC;
    }

    return 0;
}

static int filter_stmt_label_set(filter_stmt_t *stmt, regmatch_t *re_match, size_t re_match_num,
                                 char *str, char str_len, metric_family_t *fam, metric_t *m)
{
    if ((stmt->stmt_label_set.value.num == 1) &&
        (stmt->stmt_label_set.value.ptr[0].type == FILTER_SUB_STR)) {
        label_set_add(&m->label, true, stmt->stmt_label_set.label,
                                       stmt->stmt_label_set.value.ptr[0].str);
    } else {
        strbuf_t buf = STRBUF_CREATE;
        filter_replace(&stmt->stmt_label_set.value, &buf, re_match, re_match_num, str, str_len,
                                                          fam->name, &m->label);
        if (buf.ptr != NULL)
            label_set_add(&m->label, true, stmt->stmt_label_set.label, buf.ptr);
        strbuf_destroy(&buf);
    }

    return 0;
}

static int filter_stmt_label_rename(filter_stmt_t *stmt, regmatch_t *re_match, size_t re_match_num,
                                    char *str, char str_len, metric_family_t *fam, metric_t *m)
{
    if ((stmt->stmt_label_rename.to.num == 1) &&
        (stmt->stmt_label_rename.to.ptr[0].type == FILTER_SUB_STR)) {
        label_set_rename(&m->label, stmt->stmt_label_rename.from,
                                    stmt->stmt_label_rename.to.ptr[0].str);
    } else {
        strbuf_t buf = STRBUF_CREATE;
        filter_replace(&stmt->stmt_label_rename.to, &buf, re_match, re_match_num, str, str_len,
                                                          fam->name, &m->label);
        if (buf.ptr != NULL)
            label_set_rename(&m->label, stmt->stmt_label_rename.from, buf.ptr);
        strbuf_destroy(&buf);
    }

    return 0;
}

static int filter_process_stmt_match(filter_stmt_t *root, uint64_t *flags,
                                     metric_family_t *fam, metric_t *m)
{
    regmatch_t re_match[10];
    size_t re_match_num = STATIC_ARRAY_SIZE(re_match);
    regoff_t str_len = 0;
    char *str = NULL;

    if (root->type == FILTER_STMT_TYPE_METRIC_MATCH) {
        if (regexec(root->stmt_match.regex, fam->name, re_match_num, re_match, 0) != 0)
            return 0;
        str = strdup(fam->name);
        if (str == NULL) {
            ERROR("strdup failed.");
            return -1;
        }
        str_len = strlen(str);
    } else if (root->type == FILTER_STMT_TYPE_LABEL_VALUE_MATCH) {
        label_pair_t *pair = label_set_read(m->label, root->stmt_match.label);
        if (pair == NULL)
            return 0;
        if (regexec(root->stmt_match.regex, pair->value, re_match_num, re_match, 0) != 0)
            return 0;
        str = strdup(pair->value);
        if (str == NULL) {
            ERROR("strdup failed.");
            return -1;
        }
        str_len = strlen(str);
    } else {
        return 0;
    }

    filter_stmt_t *stmt = root->stmt_match.stmt;
    while (stmt != NULL) {
        switch(stmt->type) {
        case FILTER_STMT_TYPE_METRIC_RENAME:
            filter_stmt_metric_rename(stmt, re_match, re_match_num, str, str_len, flags, fam, m);
            break;
        case FILTER_STMT_TYPE_LABEL_SET:
            filter_stmt_label_set(stmt, re_match, re_match_num, str, str_len, fam, m);
            break;
        case FILTER_STMT_TYPE_LABEL_UNSET:
            label_set_add(&m->label, true, stmt->stmt_label_unset.label, NULL);
            break;
        case FILTER_STMT_TYPE_LABEL_RENAME:
            filter_stmt_label_rename(stmt, re_match, re_match_num, str, str_len, fam, m);
            break;
        default:
            break;
        }
        stmt = stmt->next;
    }

    free(str);

    return 0;
}

filter_result_t filter_process_stmt(filter_stmt_t *stmt, uint64_t *flags,
                                                         metric_family_t *fam, metric_t *m)
{
    if ((fam == NULL) || (m == NULL))
        return FILTER_RESULT_CONTINUE;

    filter_result_t result = FILTER_RESULT_CONTINUE;

    while (stmt != NULL) {
        switch(stmt->type) {
        case FILTER_STMT_TYPE_IF:
        case FILTER_STMT_TYPE_UNLESS: {
            bool match = metric_match_cmp(&stmt->stmt_if.match, fam->name, &m->label);
            if (stmt->type == FILTER_STMT_TYPE_UNLESS)
                match = !match;
            if (match) {
                result = filter_process_stmt(stmt->stmt_if.stmt, flags, fam, m);
                if (result != FILTER_RESULT_CONTINUE)
                    return result;
            } else {
                filter_stmt_t *next = stmt->stmt_if.next;
                while (next != NULL) {
                    if (next->type == FILTER_STMT_TYPE_ELIF) {
                        if (metric_match_cmp(&next->stmt_if.match, fam->name, &m->label)) {
                            result = filter_process_stmt(next->stmt_if.stmt, flags, fam, m);
                            if (result != FILTER_RESULT_CONTINUE)
                                return result;
                            break;
                        }
                    } else if (next->type == FILTER_STMT_TYPE_ELSE) {
                        result = filter_process_stmt(next->stmt_if.stmt, flags, fam, m);
                        if (result != FILTER_RESULT_CONTINUE)
                            return result;
                        break;
                    }
                    next = next->next;
                }
            }
        }   break;
        case FILTER_STMT_TYPE_ELIF:
            break;
        case FILTER_STMT_TYPE_ELSE:
            break;
        case FILTER_STMT_TYPE_DROP:
            return FILTER_RESULT_DROP;
            break;
        case FILTER_STMT_TYPE_STOP:
            return FILTER_RESULT_STOP;
            break;
        case FILTER_STMT_TYPE_RETURN:
            return FILTER_RESULT_RETURN;
            break;
        case FILTER_STMT_TYPE_CALL:
            result = filter_process_stmt(stmt->stmt_call.filter->ptr, flags, fam, m);
            if ((result == FILTER_RESULT_DROP) || (result == FILTER_RESULT_STOP))
                return result;
            break;
        case FILTER_STMT_TYPE_WRITE:
            filter_write(stmt, fam, m);
            break;
        case FILTER_STMT_TYPE_METRIC_RENAME:
            filter_stmt_metric_rename(stmt, NULL, 0, NULL, 0, flags, fam, m);
            break;
        case FILTER_STMT_TYPE_LABEL_SET:
            filter_stmt_label_set(stmt, NULL, 0, NULL, 0, fam, m);
            break;
        case FILTER_STMT_TYPE_LABEL_UNSET:
            label_set_add(&m->label, true, stmt->stmt_label_unset.label, NULL);
            break;
        case FILTER_STMT_TYPE_LABEL_RENAME:
            filter_stmt_label_rename(stmt, NULL, 0, NULL, 0, fam, m);
            break;
        case FILTER_STMT_TYPE_LABEL_ALLOW:
        case FILTER_STMT_TYPE_LABEL_IGNORE:
            filter_list_labels(&stmt->stmt_list, stmt->type, &m->label);
            break;
        case FILTER_STMT_TYPE_METRIC_SUB:
        case FILTER_STMT_TYPE_METRIC_GSUB: {
            bool global = stmt->type == FILTER_STMT_TYPE_METRIC_GSUB ? true : false;
            strbuf_t buf = STRBUF_CREATE;
            int status = filter_sub(stmt->stmt_sub.regex, &buf, fam->name,
                                    &stmt->stmt_sub.replace, fam->name, &m->label, global);
            if (status > 0) {
                if (buf.ptr != NULL) {
                    char *metric_name = strdup(buf.ptr);
                    if (metric_name != NULL) {
                        if (*flags & FILTER_FAM_METRIC_ALLOC)
                            free(fam->name);
                        fam->name = metric_name;
                        *flags |= FILTER_FAM_METRIC_ALLOC;
                    }
                }
            }
            strbuf_destroy(&buf);
        }   break;
        case FILTER_STMT_TYPE_LABEL_SUB:
        case FILTER_STMT_TYPE_LABEL_GSUB: {
            bool global = stmt->type == FILTER_STMT_TYPE_LABEL_GSUB ? true : false;
            strbuf_t buf = STRBUF_CREATE;
            for(size_t i = 0; i < m->label.num; i++) {
                char *name = m->label.ptr[i].name;
                int status = filter_sub(stmt->stmt_sub.regex, &buf, name,
                                        &stmt->stmt_sub.replace, fam->name, &m->label, global);
                if (status > 0) {
                    if (buf.ptr != NULL) {
                        label_pair_t *pair = label_set_read(m->label, buf.ptr);
                        if (pair == NULL) {
                            char *new_name = strdup(buf.ptr);
                            if (new_name != NULL) {
                                free(m->label.ptr[i].name);
                                m->label.ptr[i].name = new_name;
                                label_set_qsort(&m->label);
                            }
                        }
                    }
                }
                strbuf_reset(&buf);
            }
            strbuf_destroy(&buf);
        }   break;
        case FILTER_STMT_TYPE_LABEL_VALUE_SUB:
        case FILTER_STMT_TYPE_LABEL_VALUE_GSUB: {
            bool global = stmt->type == FILTER_STMT_TYPE_LABEL_VALUE_GSUB ? true : false;
            strbuf_t buf = STRBUF_CREATE;
            label_pair_t *pair = label_set_read(m->label, stmt->stmt_sub.label);
            if (pair != NULL) {
                int status = filter_sub(stmt->stmt_sub.regex, &buf, pair->value,
                                        &stmt->stmt_sub.replace, fam->name, &m->label, global);
                if (status > 0) {
                    if (buf.ptr != NULL) {
                        char *new_value = strdup(buf.ptr);
                        if (new_value != NULL) {
                            free(pair->value);
                            pair->value = new_value;
                        }
                    }
                }
            }
            strbuf_destroy(&buf);
        }   break;
        case FILTER_STMT_TYPE_METRIC_MATCH:
        case FILTER_STMT_TYPE_LABEL_VALUE_MATCH:
            filter_process_stmt_match(stmt, flags, fam, m);
            break;
        }
        stmt = stmt->next;
    }

    return result;
}

int filter_process(filter_t *filter, metric_family_list_t *faml)
{
    if ((filter == NULL) || (faml == NULL))
        return -1;

    if ((faml->pos != 1) || (faml->ptr == NULL))
        return -1;

    metric_family_t *fam = faml->ptr[0];

    size_t remain = 0;

    for (size_t i = 0; i < fam->metric.num; i++) {
        metric_family_t sfam  = {
            .name = fam->name,
            .help = NULL,
            .unit = NULL,
            .type = fam->type,
        };
        uint64_t flags = 0;

        filter_result_t result = filter_process_stmt(filter->ptr, &flags, &sfam,
                                                                  &fam->metric.ptr[i]);
        if (result == FILTER_RESULT_DROP) {
            metric_reset(&fam->metric.ptr[i], fam->type);
            continue;
        }

        if (flags) {
            metric_family_t *new_fam = NULL;
            size_t j = 0;
            for (j = 0; j < faml->pos; j++) {
                if (strcmp(sfam.name, faml->ptr[j]->name ) == 0) {
                    new_fam = faml->ptr[j];
                    break;
                }
            }

            if ((j == 0) && (new_fam != NULL)) {
                if (flags & FILTER_FAM_METRIC_ALLOC)
                    free(sfam.name);
                remain++;
                continue;
            }

            if (new_fam == NULL) {
                new_fam = calloc(1, sizeof(*new_fam));
                if (new_fam == NULL) {
                    ERROR("calloc failed.");
                    continue;
                }

                if (flags & FILTER_FAM_METRIC_ALLOC) {
                    new_fam->name = sfam.name;
                } else {
                    new_fam->name = strdup(sfam.name);
                    if (new_fam->name == NULL) {
                        ERROR("strdup failed.");
                        free(new_fam);
                        continue;
                    }
                }

                new_fam->type = fam->type;

                metric_family_list_append(faml, new_fam);
            } else {
                if (flags & FILTER_FAM_METRIC_ALLOC)
                    free(sfam.name);
            }

            metric_list_append(&new_fam->metric, fam->metric.ptr[i]);
            memset(&fam->metric.ptr[i], 0, sizeof(fam->metric.ptr[i]));

        } else {
            remain++;
        }
    }

    if (remain == 0) {
        free(fam->metric.ptr);
        fam->metric.ptr = NULL;
        fam->metric.num = 0;
    }

    if ((faml->pos == 1) && ((remain == fam->metric.num) || (remain == 0)))
        return 0;

    if ((remain > 0) && (remain != fam->metric.num)) {
        metric_t *metric = calloc(remain, sizeof(*metric));
        size_t n = 0;
        for (size_t i = 0; i < fam->metric.num; i++) {
            if (fam->metric.ptr[i].time != 0) {
                metric[n].value = fam->metric.ptr[i].value;
                metric[n].time = fam->metric.ptr[i].time;
                metric[n].interval = fam->metric.ptr[i].interval;
                metric[n].label = fam->metric.ptr[i].label;
                n++;
            }
        }

        free(fam->metric.ptr);
        fam->metric.ptr = metric;
        fam->metric.num = n;
    }

    return 0;
}
