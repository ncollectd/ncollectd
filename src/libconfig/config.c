// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2006,2007  Florian Forster
// SPDX-FileContributor: Florian Forster <octo at collectd.org>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libconfig/config.h"
#include "libconfig/aux_types.h"
#define YYSTYPE  CONFIG_YYSTYPE
#define YYLTYPE  CONFIG_YYLTYPE
#include "libconfig/parser.h"
#include "libconfig/scanner.h"

// extern FILE *config_yyin;
// extern int config_yyparse(void);

config_item_t *ci_root;
config_file_t *c_file;

static config_file_t *config_get_file_ref(config_file_t *file)
{
    if (file == NULL)
        return NULL;
    file->refcnt++;
    return file;
}

static config_item_t *config_parse_fh(FILE *fh)
{
    if (fh == NULL)
        return NULL;

    yyscan_t scanner;
    config_status_t ostatus = {0};

    config_yylex_init(&scanner);
//  config_yyset_debug(1, scanner);

    config_yyrestart(fh, scanner);
    config_yyset_in(fh, scanner);

    int status = config_yyparse(scanner, &ostatus);
    if (status != 0) {
        fprintf(stderr, "yyparse failed %i: %s\n", status, ostatus.error);
        return NULL;
    }

    config_item_t *ret = ci_root;
    ci_root = NULL;
    config_yyset_in((FILE *)0, scanner);

    config_yylex_destroy(scanner);

    return ret;
}

config_item_t *config_parse_file(const char *file)
{
    size_t file_len = strlen(file);
    c_file = malloc(sizeof(config_file_t) + file_len + 1);
    if (c_file == NULL) {
        fprintf(stderr, "'%s' malloc failed: %s\n", file, strerror(errno));
        return NULL;
    }
    c_file->refcnt = 0;
    memcpy(c_file->name, file, file_len + 1);

    FILE *fh = fopen(file, "r");
    if (fh == NULL) {
        fprintf(stderr, "fopen (%s) failed: %s\n", file, strerror(errno));
        free(c_file);
        c_file = NULL;
        return NULL;
    }

    config_item_t *ret = config_parse_fh(fh);
    fclose(fh);

    if (c_file->refcnt == 0)
        free(c_file);

    c_file = NULL;

    return ret;
}

config_item_t *config_clone(const config_item_t *ci_orig) // FIXME
{

    config_item_t *ci_copy = calloc(1, sizeof(*ci_copy));
    if (ci_copy == NULL) {
        fprintf(stderr, "calloc failed.\n");
        return NULL;
    }
    ci_copy->values = NULL;
    ci_copy->parent = NULL;
    ci_copy->children = NULL;

    ci_copy->key = strdup(ci_orig->key);
    if (ci_copy->key == NULL) {
        fprintf(stderr, "strdup failed.\n");
        free(ci_copy);
        return NULL;
    }

    ci_copy->file = config_get_file_ref(ci_orig->file); // FIXME

    if (ci_orig->values_num > 0) {
        ci_copy->values = calloc(ci_orig->values_num, sizeof(*ci_copy->values));
        if (ci_copy->values == NULL) {
            fprintf(stderr, "calloc failed.\n");
            free(ci_copy->key);
            free(ci_copy);
            return NULL;
        }
        ci_copy->values_num = ci_orig->values_num;

        for (int i = 0; i < ci_copy->values_num; i++) {
            ci_copy->values[i].type = ci_orig->values[i].type;
            if (ci_copy->values[i].type == CONFIG_TYPE_STRING) {
                ci_copy->values[i].value.string = strdup(ci_orig->values[i].value.string);
                if (ci_copy->values[i].value.string == NULL) {
                    fprintf(stderr, "strdup failed.\n");
                    config_free(ci_copy);
                    return NULL;
                }
            } else {
                ci_copy->values[i].value = ci_orig->values[i].value;
            }
        }
    }

    if (ci_orig->children_num > 0) {
        ci_copy->children = calloc(ci_orig->children_num, sizeof(*ci_copy->children));
        if (ci_copy->children == NULL) {
            fprintf(stderr, "calloc failed.\n");
            config_free(ci_copy);
            return NULL;
        }
        ci_copy->children_num = ci_orig->children_num;

        for (int i = 0; i < ci_copy->children_num; i++) {
            config_item_t *child;

            child = config_clone(ci_orig->children + i);
            if (child == NULL) {
                config_free(ci_copy);
                return NULL;
            }
            child->parent = ci_copy;
            ci_copy->children[i] = *child;
            free(child);
        }
    }

    return ci_copy;
}

static void config_dump_config_item(FILE *fh, int level, config_item_t *ci)
{
    if (ci == NULL)
        return;

    if (ci->key == NULL)
        return;

    for (int i = 0; i < level; i++) {
        fputs("    ", fh);
    }

    fputs(ci->key, fh);

    for (int i = 0; i < ci->values_num; i++) {
        switch(ci->values[i].type) {
        case CONFIG_TYPE_STRING:
            fprintf(fh, " \"%s\"", ci->values[i].value.string); // FIXME escape "
            break;
        case CONFIG_TYPE_NUMBER:
            fprintf(fh, " %f", ci->values[i].value.number);
            break;
        case CONFIG_TYPE_BOOLEAN:
            fprintf(fh, " %s", ci->values[i].value.boolean ? "true" : "false");
            break;
        case CONFIG_TYPE_REGEX:
            fprintf(fh, " /%s/", ci->values[i].value.string); // FIXME escape /
            break;
        }
    }

    if (ci->children != NULL)
        fputs(" {", fh);

    fputc('\n', fh);


    for (int i = 0; i < ci->children_num; i++) {
        config_dump_config_item(fh, level + 1, ci->children + i);
    }

    if (ci->children != NULL) {
        for (int i = 0; i < level; i++) {
            fputs("    ", fh);
        }
        fputs("}\n", fh);
    }

}

void config_dump(FILE *fh, config_item_t *ci)
{
    if (ci == NULL)
        return;

    for (int i = 0; i < ci->children_num; i++) {
        config_dump_config_item(fh, 0, ci->children + i);
    }
}

static void config_free_all(config_item_t *ci)
{
    if (ci == NULL)
        return;

    if (ci->key != NULL)
        free(ci->key);

    if (ci->values != NULL) {
        for (int i = 0; i < ci->values_num; i++) {
            if (((ci->values[i].type == CONFIG_TYPE_STRING) ||
                 (ci->values[i].type == CONFIG_TYPE_REGEX)) &&
                (ci->values[i].value.string != NULL)) {
                free(ci->values[i].value.string);
            }
        }
        free(ci->values);
    }

    if (ci->file != NULL) {
        ci->file->refcnt--;
        if (ci->file->refcnt <= 0)
            free(ci->file);
    }

    if (ci->children != NULL) {
        for (int i = 0; i < ci->children_num; i++) {
            config_free_all(ci->children + i);
        }
        free(ci->children);
    }
}

void config_free(config_item_t *ci)
{
    config_free_all(ci);
    free(ci);
}
