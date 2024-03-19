// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/config.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <regex.h>

typedef enum {
    EXCLIST_ENTRY_TYPE_STRING,
    EXCLIST_ENTRY_TYPE_REGEX
} exclist_entry_type_t;

struct exclist_entry_s {
    exclist_entry_type_t type;
    union {
        char *string;
        regex_t *regex;
    };
};

static void exclist_entry_free(exclist_entry_t *entry)
{
    if (entry == NULL)
        return;

    switch (entry->type) {
    case EXCLIST_ENTRY_TYPE_STRING:
        free(entry->string);
        break;
    case EXCLIST_ENTRY_TYPE_REGEX:
        if (entry->regex != NULL) {
            regfree(entry->regex);
            free(entry->regex);
        }
        break;
    }

    free(entry);
}

static exclist_entry_t *exclist_entry_alloc_string(const char *value)
{
    exclist_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return NULL;
    }

    entry->string = strdup(value);
    if (entry->string == NULL) {
        free(entry);
        PLUGIN_ERROR("strdup failed.");
        return NULL;
    }

    entry->type = EXCLIST_ENTRY_TYPE_STRING;
    return entry;
}

static exclist_entry_t *exclist_entry_alloc_regex(const char *value)
{
    exclist_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return NULL;
    }

    entry->regex = calloc(1, sizeof(*entry->regex));
    if (entry->regex == NULL) {
        PLUGIN_ERROR("calloc failed.");
        free(entry);
        return NULL;
    }

    int status = regcomp(entry->regex, value, REG_EXTENDED|REG_NOSUB);
    if (status != 0) {
        PLUGIN_ERROR("regcom '%s' failed: %s.", value, STRERRNO);
        free(entry->regex);
        free(entry);
        return NULL;
    }

    entry->type = EXCLIST_ENTRY_TYPE_REGEX;
    return entry;
}

static int exclist_list_add(exclist_list_t *list, exclist_entry_t *entry)
{
    exclist_entry_t **tmp = realloc(list->ptr, sizeof(exclist_entry_t *)*(list->num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }

    list->ptr = tmp;
    list->ptr[list->num] = entry;
    list->num++;
    return 0;
}

int exclist_list_remove_string(exclist_list_t *list, const char *value)
{
    bool found = false;
    size_t i = 0;
    for (i = 0; i < list->num; i++) {
        exclist_entry_t *entry = list->ptr[i];
        if (entry->type == EXCLIST_ENTRY_TYPE_STRING) {
            if (strcmp(entry->string, value) == 0) {
                found = true;
                break;
            }
        }
    }

    if (!found)
        return -1;

    exclist_entry_free(list->ptr[i]);
    list->ptr[i] = NULL;
    list->num--;

    for (size_t j = i; j < list->num; j++) {
        list->ptr[j] =  list->ptr[j+1];
    }

    return 0;
}

int exclist_add_incl_string(exclist_t *excl, const char *value)
{
    exclist_entry_t *entry = exclist_entry_alloc_string(value);
    if (entry == NULL)
        return -1;

    int status = exclist_list_add(&excl->incl, entry);
    if (status != 0) {
        exclist_entry_free(entry);
        return -1;
    }

    return 0;
}

int exclist_remove_incl_string(exclist_t *excl, const char *value)
{
   return exclist_list_remove_string(&excl->incl, value);
}

int exclist_add_excl_string(exclist_t *excl, const char *value)
{
    exclist_entry_t *entry = exclist_entry_alloc_string(value);
    if (entry == NULL)
        return -1;

    int status = exclist_list_add(&excl->excl, entry);
    if (status != 0) {
        exclist_entry_free(entry);
        return -1;
    }
    return 0;
}

int exclist_remove_excl_string(exclist_t *excl, const char *value)
{
   return exclist_list_remove_string(&excl->excl, value);
}

int cf_util_exclist(const config_item_t *ci, exclist_t *excl)
{
    if ((ci == NULL) || (excl == NULL))
        return EINVAL;

    if (ci->values_num == 1) {
        exclist_entry_t *entry = NULL;
        if (ci->values[0].type == CONFIG_TYPE_STRING) {
            entry = exclist_entry_alloc_string(ci->values[0].value.string);
        } else if (ci->values[0].type == CONFIG_TYPE_REGEX) {
            entry = exclist_entry_alloc_regex(ci->values[0].value.string);
        } else {
            PLUGIN_ERROR("The '%s' option in %s:%d requires a string or a regex argument.",
                         ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        if (entry == NULL)
            return -1;

        int status = exclist_list_add(&excl->incl, entry);
        if (status != 0) {
            exclist_entry_free(entry);
            return -1;
        }
        return 0;
    }

    if (ci->values_num != 2) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires one or two arguments.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[0].type != CONFIG_TYPE_STRING) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires that the first argument to be a string.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if ((ci->values[1].type != CONFIG_TYPE_STRING) && (ci->values[1].type != CONFIG_TYPE_REGEX)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires that the second argument to be a string "
                     "or regex.", ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    exclist_entry_t *entry = NULL;
    if (ci->values[1].type == CONFIG_TYPE_STRING) {
        entry = exclist_entry_alloc_string(ci->values[1].value.string);
    } else if (ci->values[1].type == CONFIG_TYPE_REGEX) {
        entry = exclist_entry_alloc_regex(ci->values[1].value.string);
    }

    if (entry == NULL)
        return -1;

    int status = 0;
    if ((strcasecmp("inc", ci->values[0].value.string) == 0) ||
        (strcasecmp("incl", ci->values[0].value.string) == 0) ||
        (strcasecmp("include", ci->values[0].value.string) == 0)) {
        status = exclist_list_add(&excl->incl, entry);
    } else if ((strcasecmp("ex", ci->values[0].value.string) == 0) ||
               (strcasecmp("excl", ci->values[0].value.string)== 0) ||
               (strcasecmp("exclude", ci->values[0].value.string)== 0)) {
        status = exclist_list_add(&excl->excl, entry);
    } else {
        PLUGIN_ERROR("The first argument of '%s' option in %s:%d must be: "
                     "'inc', 'incl', 'include', 'ex', 'excl' or 'exclude'.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        status = -1;
    }

    if (status != 0) {
        exclist_entry_free(entry);
        return -1;
    }

    return 0;
}

static inline bool exclist_entry_match(exclist_entry_t *entry, const char *value)
{
    switch (entry->type) {
    case EXCLIST_ENTRY_TYPE_STRING:
        return strcmp(entry->string, value) == 0;
        break;
    case EXCLIST_ENTRY_TYPE_REGEX:
        return  regexec(entry->regex, value, 0, NULL, 0) == 0;
        break;
    }

    return false;
}

bool exclist_match(exclist_t *excl, const char *value)
{
    if ((excl->incl.num == 0) && (excl->excl.num == 0)) {
        return true;
    } else if ((excl->incl.num > 0) && (excl->excl.num == 0)) {
        for (size_t i = 0; i < excl->incl.num; i++) {
            if (exclist_entry_match(excl->incl.ptr[i], value))
                return true;
        }
        return false;
    } else if ((excl->incl.num == 0) && (excl->excl.num > 0)) {
        for (size_t i = 0; i < excl->excl.num; i++) {
            if (exclist_entry_match(excl->excl.ptr[i], value))
                return false;
        }
        return true;
    } else if ((excl->incl.num > 0) && (excl->excl.num > 0)) {
        bool found = false;
        for (size_t i = 0; i < excl->incl.num; i++) {
            if (exclist_entry_match(excl->incl.ptr[i], value)) {
                found = true;
                break;
            }
        }

        if (!found)
            return false;

        for (size_t i = 0; i < excl->excl.num; i++) {
            if (exclist_entry_match(excl->excl.ptr[i], value))
                return false;
        }
        return true;
    }

    return true;
}

void exclist_reset(exclist_t *excl)
{
    for (size_t i = 0; i < excl->incl.num; i++) {
        exclist_entry_free(excl->incl.ptr[i]);
    }
    free(excl->incl.ptr);
    excl->incl.ptr = NULL;
    excl->incl.num = 0;

    for (size_t i = 0; i < excl->excl.num; i++) {
        exclist_entry_free(excl->excl.ptr[i]);
    }
    free(excl->excl.ptr);
    excl->excl.ptr = NULL;
    excl->excl.num = 0;
}
