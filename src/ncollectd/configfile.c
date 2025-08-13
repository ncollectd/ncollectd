// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2005-2011  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Sebastian tokkee Harl <sh at tokkee.org>

#include "ncollectd.h"
#include "globals.h"
#include "configfile.h"
#include "filter.h"
#include "plugin_internal.h"
#include "libutils/common.h"
#include "libutils/config.h"
#include "libmetric/label_set.h"

#ifdef HAVE_WORDEXP_H
#include <wordexp.h>
#endif /* HAVE_WORDEXP_H */

#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif /* HAVE_FNMATCH_H */

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#define ESCAPE_NULL(str) ((str) == NULL ? "(null)" : (str))

typedef struct cf_callback_s {
    char *type;
    int (*callback)(config_item_t *);
    plugin_ctx_t ctx;
    struct cf_callback_s *next;
} cf_callback_t;

typedef struct cf_value_map_s {
    const char *key;
    int (*func)(config_item_t *);
} cf_value_map_t;

typedef struct cf_global_option_s {
    const char *key;
    char *value;
    bool from_cli; /* value set from CLI */
    const char *def;
} cf_global_option_t;

typedef struct {
    char *name;
    int num;
} cf_cpumap_t;

static int cf_cpumap_num;
static cf_cpumap_t *cf_cpumap;

/* Prototypes of callback functions */
static int dispatch_value_plugindir(config_item_t *ci);
static int dispatch_loadplugin(config_item_t *ci);
static int dispatch_block_plugin(config_item_t *ci);
static int dispatch_label(config_item_t *ci);
static int dispatch_cpumap(config_item_t *ci);

/* Private variables */
static cf_callback_t *callback_head;

static cf_value_map_t cf_value_map[] = {
    {"plugin-dir",  dispatch_value_plugindir},
    {"load-plugin", dispatch_loadplugin     },
    {"plugin",      dispatch_block_plugin   },
    {"label",       dispatch_label          },
    {"cpu-map",     dispatch_cpumap         }
};
static int cf_value_map_num = STATIC_ARRAY_SIZE(cf_value_map);

static cf_global_option_t cf_global_options[] = {
    {"base-dir",                NULL, 0, PKGLOCALSTATEDIR  },
    {"pid-file",                NULL, 0, PIDFILE           },
    {"hostname",                NULL, 0, NULL              },
    {"fqdn-lookup",             NULL, 0, "true"            },
    {"interval",                NULL, 0, NULL              },
    {"read-threads",            NULL, 0, "5"               },
    {"write-queue-limit-high",  NULL, 0, NULL              },
    {"write-queue-limit-low",   NULL, 0, NULL              },
    {"notify-queue-limit-high", NULL, 0, NULL              },
    {"notify-queue-limit-low",  NULL, 0, NULL              },
    {"timeout",                 NULL, 0, "2"               },
    {"auto-load-plugin",        NULL, 0, "false"           },
    {"collect-internal-stats",  NULL, 0, "false"           },
    {"pre-cache-filter",        NULL, 0, "pre-cache"       },
    {"post-cache-filter",       NULL, 0, "post-cache"      },
    {"max-read-interval",       NULL, 0, "86400"           },
    {"normalize-interval",      NULL, 0, "false"           },
    {"socket-file",             NULL, 0, UNIXSOCKETPATH    },
    {"socket-group",            NULL, 0, NCOLLECTD_GRP_NAME},
    {"socket-perms",            NULL, 0, "0770"            },
    {"socket-delete",           NULL, 0, "false"           },
    {"proc-path",               NULL, 0, "/proc"           },
    {"sys-path",                NULL, 0, "/sys"            }
};
static int cf_global_options_num = STATIC_ARRAY_SIZE(cf_global_options);

static int dispatch_global_option(const config_item_t *ci)
{
    if (ci->values_num != 1) {
        ERROR("configfile: Global option '%s' in %s:%d needs exactly one argument.",
              ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[0].type == CONFIG_TYPE_STRING) {
        return global_option_set(ci->key, ci->values[0].value.string, 0);
    } else if (ci->values[0].type == CONFIG_TYPE_NUMBER) {
        char tmp[128];
        ssnprintf(tmp, sizeof(tmp), "%lf", ci->values[0].value.number);
        return global_option_set(ci->key, tmp, 0);
    } else if (ci->values[0].type == CONFIG_TYPE_BOOLEAN) {
        if (ci->values[0].value.boolean)
            return global_option_set(ci->key, "true", 0);
        else
            return global_option_set(ci->key, "false", 0);
    }

    ERROR("configfile: Global option '%s' argument has unknown type in %s:%d.",
          ci->key, cf_get_file(ci), cf_get_lineno(ci));

    return -1;
}

static int dispatch_value_plugindir(config_item_t *ci)
{
    assert(strcasecmp(ci->key, "plugin-dir") == 0);

    if (ci->values_num != 1 || ci->values[0].type != CONFIG_TYPE_STRING) {
        ERROR("configfile: The 'plugin-dir' option in %s:%d needs exactly one string argument.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    plugin_set_dir(ci->values[0].value.string);
    return 0;
}

static int dispatch_loadplugin(config_item_t *ci)
{
    bool global = false;

    assert(strcasecmp(ci->key, "load-plugin") == 0);

    if (ci->values_num != 1 || ci->values[0].type != CONFIG_TYPE_STRING) {
        ERROR("configfile: The 'load-plugin' block in %s:%d needs exactly one string argument.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    const char *name = ci->values[0].value.string;

    bool normalize = IS_TRUE(global_option_get("normalize-interval"));

    /* default to the global interval set before loading this plugin */
    plugin_ctx_t ctx = {
            .interval = cf_get_default_interval(),
            .name = strdup(name),
            .normalize_interval = normalize,
    };
    if (ctx.name == NULL)
        return ENOMEM;

    int status = 0;
    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("globals", child->key) == 0) {
            status = cf_util_get_boolean(child, &global);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &ctx.interval);
        } else if (strcasecmp("normalize-interval", child->key) == 0) {
            status = cf_util_get_boolean(child, &ctx.normalize_interval);
        } else {
            ERROR("Unknown load-plugin option '%s' for plugin '%s' in %s:%d",
                  child->key, name, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        free(ctx.name);
        return -1;
    }

    plugin_ctx_t old_ctx = plugin_set_ctx(ctx);
    int ret_val = plugin_load(name, global);
    /* reset to the "global" context */
    plugin_set_ctx(old_ctx);

    return ret_val;
}

static int dispatch_value(config_item_t *ci)
{
    for (int i = 0; i < cf_value_map_num; i++) {
        if (strcasecmp(cf_value_map[i].key, ci->key) == 0) {
            return cf_value_map[i].func(ci);
        }
    }

    for (int i = 0; i < cf_global_options_num; i++) {
        if (strcasecmp(cf_global_options[i].key, ci->key) == 0) {
            return dispatch_global_option(ci);
        }
    }

    ERROR("Unknown global option '%s' in %s:%d",
           ci->key, cf_get_file(ci), cf_get_lineno(ci));

    return -1;
}

static int dispatch_block_plugin(config_item_t *ci)
{
    assert(strcasecmp(ci->key, "plugin") == 0);

    if (ci->values_num < 1) {
        ERROR("configfile: The 'plugin' block in %s:%d requires arguments.",
               cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }
    if (ci->values[0].type != CONFIG_TYPE_STRING) {
        ERROR("configfile: First argument of 'plugin' block in %s:%d should be a string.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    char const *plugin_name = ci->values[0].value.string;
    bool plugin_loaded = plugin_is_loaded(plugin_name);

    if (!plugin_loaded && IS_TRUE(global_option_get("auto-load-plugin"))) {
        plugin_ctx_t ctx = {0};

        /* default to the global interval set before loading this plugin */
        ctx.interval = cf_get_default_interval();
        ctx.name = strdup(plugin_name);
        ctx.normalize_interval = IS_TRUE(global_option_get("normalize-interval"));

        plugin_ctx_t old_ctx = plugin_set_ctx(ctx);
        int status = plugin_load(plugin_name, /* flags = */ false);
        /* reset to the "global" context */
        plugin_set_ctx(old_ctx);

        if (status != 0) {
            ERROR("Automatically loading plugin '%s' failed with status %i.", plugin_name, status);
            return status;
        }
        plugin_loaded = true;
    }

    if (!plugin_loaded) {
        WARNING("There is configuration for the '%s' plugin, but the plugin isn't "
                "loaded. Please check your configuration.", plugin_name);
        return -1;
    }

    for (cf_callback_t *cb = callback_head; cb != NULL; cb = cb->next) {
        if (strcasecmp(plugin_name, cb->type) == 0) {
            plugin_ctx_t old_ctx = plugin_set_ctx(cb->ctx);
            int ret_val = (cb->callback(ci));
            plugin_set_ctx(old_ctx);
            return ret_val;
        }
    }

    if (ci->children_num > 0) {
        WARNING("Found a configuration for the '%s' plugin, but "
                "the plugin didn't register a configuration callback.", plugin_name);
        return -1;
    }

    return 0;
}

static int dispatch_block(config_item_t *ci)
{
    if (strcasecmp(ci->key, "load-plugin") == 0)
        return dispatch_loadplugin(ci);
    else if (strcasecmp(ci->key, "plugin") == 0)
        return dispatch_block_plugin(ci);
    else if (strcasecmp(ci->key, "filter") == 0)
        return filter_global_configure(ci);
    else
        return -1;

    return 0;
}

static int dispatch_label(config_item_t *ci)
{
    assert(strcasecmp(ci->key, "label") == 0);

    if (ci->values_num != 2) {
        ERROR("configfile: The 'label' in %s:%d option requires two arguments.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }
    if ((ci->values[0].type != CONFIG_TYPE_STRING) ||
        (ci->values[1].type != CONFIG_TYPE_STRING)) {
        ERROR("configfile: The arguments of 'label' option in %s:%d should be strings.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return label_set_add(&labels_g, true, ci->values[0].value.string, ci->values[1].value.string);
}

static int dispatch_cpumap(config_item_t *ci)
{
    assert(strcasecmp(ci->key, "cpu-map") == 0);

    if (ci->values_num != 2) {
        ERROR("configfile: The 'cpu-map' in %s:%d option requires two arguments.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }
    if (ci->values[0].type != CONFIG_TYPE_STRING) {
        ERROR("configfile: The first argument of 'cpu-map' option in %s:%d must be a string.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (ci->values[1].type != CONFIG_TYPE_NUMBER) {
        ERROR("configfile: The seconds argument of 'cpu-map' option in %s:%d must be a number.",
              cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    cf_cpumap_t *tmp = realloc(cf_cpumap, sizeof(*tmp) * (cf_cpumap_num+1));
    if (tmp == NULL) {
        ERROR("configfile: realloc failed.");
        return -1;
    }

    cf_cpumap = tmp;
    cf_cpumap[cf_cpumap_num].name = strdup(ci->values[0].value.string);
    if (cf_cpumap[cf_cpumap_num].name == NULL) {
        ERROR("configfile: strdup failed.");
        return -1;
    }

    cf_cpumap[cf_cpumap_num].num = ci->values[1].value.number;
    cf_cpumap_num++;
    return 0;
}

static int cf_ci_replace_child(config_item_t *dst, config_item_t *src, int offset)
{
    assert(offset >= 0);
    assert(dst->children_num > offset);

    /* Free the memory used by the replaced child. Usually that's the
     * Include "blah"' statement. */
    config_item_t *temp = dst->children + offset;
    for (int i = 0; i < temp->values_num; i++) {
        if (temp->values[i].type == CONFIG_TYPE_STRING) {
            free(temp->values[i].value.string);
        }
    }
    free(temp->values);
    temp = NULL;

    /* If (src->children_num == 0) the array size is decreased. If offset
     * is _not_ the last element, (offset < (dst->children_num - 1)), then
     * we need to move the trailing elements before resizing the array. */
    if ((src->children_num == 0) && (offset < (dst->children_num - 1))) {
        int nmemb = dst->children_num - (offset + 1);
        memmove(dst->children + offset, dst->children + offset + 1,
                        sizeof(config_item_t) * nmemb);
    }

    /* Resize the memory containing the children to be big enough to hold
     * all children. */
    if (dst->children_num + src->children_num - 1 == 0) {
        dst->children_num = 0;
        return 0;
    }

    temp = realloc(dst->children, sizeof(config_item_t) *
                                  (dst->children_num + src->children_num - 1));
    if (temp == NULL) {
        ERROR("configfile: realloc failed.");
        return -1;
    }
    dst->children = temp;

    /* If there are children behind the include statement, and they have
     * not yet been moved because (src->children_num == 0), then move them
     * to the end of the list, so that the new children have room before
     * them. */
    if ((src->children_num > 0) && ((dst->children_num - (offset + 1)) > 0)) {
        int nmemb = dst->children_num - (offset + 1);
        int old_offset = offset + 1;
        int new_offset = offset + src->children_num;

        memmove(dst->children + new_offset, dst->children + old_offset,
                        sizeof(config_item_t) * nmemb);
    }

    /* Last but not least: If there are new children, copy them to the
     * memory reserved for them. */
    if (src->children_num > 0)
        memcpy(dst->children + offset, src->children, sizeof(config_item_t) * src->children_num);

    /* Update the number of children. */
    dst->children_num += (src->children_num - 1);

    return 0;
}

static int cf_ci_append_children(config_item_t *dst, config_item_t *src)
{
    config_item_t *temp;

    if ((src == NULL) || (src->children_num == 0))
        return 0;

    temp = realloc(dst->children, sizeof(config_item_t) * (dst->children_num + src->children_num));
    if (temp == NULL) {
        ERROR("configfile: realloc failed.");
        return -1;
    }
    dst->children = temp;

    memcpy(dst->children + dst->children_num, src->children,
                                              sizeof(config_item_t) * src->children_num);
    dst->children_num += src->children_num;

    return 0;
}

#define CF_MAX_DEPTH 8
static config_item_t *cf_read_generic(const char *path, const char *pattern, int depth);

static int cf_include_all(config_item_t *root, int depth)
{
    for (int i = 0; i < root->children_num; i++) {
        char *pattern = NULL;

        if (strcasecmp(root->children[i].key, "include") != 0)
            continue;

        config_item_t *old = root->children + i;

        if ((old->values_num != 1) || (old->values[0].type != CONFIG_TYPE_STRING)) {
            ERROR("configfile: 'include' in %s:%d needs exactly one string argument.",
                  cf_get_file(old), cf_get_lineno(old));
            continue;
        }

        int status = 0;
        for (int j = 0; j < old->children_num; ++j) {
            config_item_t *child = old->children + j;

            if (strcasecmp(child->key, "filter") == 0) {
                status = cf_util_get_string(child, &pattern);
            } else {
                ERROR("configfile: option '%s' in %s:%d not allowed in 'include' block.",
                       child->key, cf_get_file(child), cf_get_lineno(child));
                status = -1;
            }

            if (status != 0)
                break;
        }

        if (status != 0) {
            free(pattern);
            return -1;
        }

        config_item_t *new = cf_read_generic(old->values[0].value.string, pattern, depth + 1);
        free(pattern);
        if (new == NULL)
            return -1;

        /* Now replace the i'th child in 'root' with 'new'. */
        if (cf_ci_replace_child(root, new, i) < 0) {
            free(new->values);
            free(new);
            return -1;
        }

        /* ... and go back to the new i'th child. */
        --i;

        free(new->values);
        free(new);
    }

    return 0;
}

static config_item_t *cf_read_file(const char *file, const char *pattern, int depth)
{
    assert(depth < CF_MAX_DEPTH);

    if (pattern != NULL) {
#if defined(HAVE_FNMATCH_H) && defined(HAVE_LIBGEN_H)
        char *tmp = sstrdup(file);
        char *filename = basename(tmp);

        if ((filename != NULL) && (fnmatch(pattern, filename, 0) != 0)) {
            DEBUG("configfile: Not including '%s' because it does not match pattern '%s'.",
                  filename, pattern);
            free(tmp);
            return NULL;
        }

        free(tmp);
#else
        ERROR("configfile: Cannot apply pattern filter '%s' to file '%s': "
              "functions basename() and / or fnmatch() not available.", pattern, file);
#endif
    }

    config_item_t *root = config_parse_file(file);
    if (root == NULL) {
        ERROR("configfile: Cannot read file '%s'.", file);
        return NULL;
    }

    int status = cf_include_all(root, depth);
    if (status != 0) {
        config_free(root);
        return NULL;
    }

    return root;
}

static int cf_compare_string(const void *p1, const void *p2)
{
    return strcmp((const char *)p1, (const char *)p2);
}

static config_item_t *cf_read_dir(const char *dir, const char *pattern, int depth)
{
    char **filenames = NULL;
    int filenames_num = 0;

    assert(depth < CF_MAX_DEPTH);

    DIR *dh = opendir(dir);
    if (dh == NULL) {
        ERROR("configfile: opendir failed: %s", STRERRNO);
        return NULL;
    }

    config_item_t *root = calloc(1, sizeof(*root));
    if (root == NULL) {
        ERROR("configfile: calloc failed.");
        closedir(dh);
        return NULL;
    }

    struct dirent *de;
    while ((de = readdir(dh)) != NULL) {
        if ((de->d_name[0] == '.') || (de->d_name[0] == 0))
            continue;

        char name[1024];
        int status = ssnprintf(name, sizeof(name), "%s/%s", dir, de->d_name);
        if ((status < 0) || ((size_t)status >= sizeof(name))) {
            ERROR("configfile: Not including '%s/%s' because its name is too long.",
                   dir, de->d_name);
            closedir(dh);
            for (int i = 0; i < filenames_num; ++i)
                free(filenames[i]);
            free(filenames);
            free(root);
            return NULL;
        }

        ++filenames_num;
        char **tmp = realloc(filenames, filenames_num * sizeof(*filenames));
        if (tmp == NULL) {
            ERROR("configfile: realloc failed.");
            closedir(dh);
            for (int i = 0; i < filenames_num - 1; ++i)
                free(filenames[i]);
            free(filenames);
            free(root);
            return NULL;
        }
        filenames = tmp;

        filenames[filenames_num - 1] = sstrdup(name);
    }

    if (filenames == NULL) {
        closedir(dh);
        return root;
    }

    qsort((void *)filenames, filenames_num, sizeof(*filenames), cf_compare_string);

    for (int i = 0; i < filenames_num; ++i) {
        char *name = filenames[i];
        config_item_t *temp = cf_read_generic(name, pattern, depth);
        if (temp == NULL) {
            /* An error should already have been reported. */
            free(name);
            continue;
        }

        cf_ci_append_children(root, temp);
        free(temp->children);
        free(temp);

        free(name);
    }

    closedir(dh);
    free(filenames);
    return root;
}

/*
 * cf_read_generic
 *
 * Path is stat'ed and either cf_read_file or cf_read_dir is called
 * accordingly.
 *
 * There are two versions of this function: If 'wordexp' exists shell wildcards
 * will be expanded and the function will include all matches found. If
 * 'wordexp' (or, more precisely, its header file) is not available the
 * simpler function is used which does not do any such expansion.
 */
#ifdef HAVE_WORDEXP_H
static config_item_t *cf_read_generic(const char *path, const char *pattern, int depth)
{
    if (depth >= CF_MAX_DEPTH) {
        ERROR("configfile: Not including '%s' because the maximum nesting depth has been reached.",
               path);
        return NULL;
    }

    wordexp_t we;
    int status = wordexp(path, &we, WRDE_NOCMD);
    if (status != 0) {
        ERROR("configfile: wordexp (%s) failed.", path);
        return NULL;
    }

    config_item_t *root = calloc(1, sizeof(*root));
    if (root == NULL) {
        ERROR("configfile: calloc failed.");
        return NULL;
    }

    /* wordexp() might return a sorted list already. That's not
     * documented though, so let's make sure we get what we want. */
    qsort((void *)we.we_wordv, we.we_wordc, sizeof(*we.we_wordv), cf_compare_string);

    for (size_t i = 0; i < we.we_wordc; i++) {
        config_item_t *temp;
        struct stat statbuf;

        const char *path_ptr = we.we_wordv[i];

        status = stat(path_ptr, &statbuf);
        if (status != 0) {
            WARNING("configfile: stat (%s) failed: %s", path_ptr, STRERRNO);
            continue;
        }

        if (S_ISREG(statbuf.st_mode)) {
            temp = cf_read_file(path_ptr, pattern, depth);
        } else if (S_ISDIR(statbuf.st_mode)) {
            temp = cf_read_dir(path_ptr, pattern, depth);
        } else {
            WARNING("configfile: %s is neither a file nor a directory.", path);
            continue;
        }

        if (temp == NULL) {
            config_free(root);
            wordfree(&we);
            return NULL;
        }

        cf_ci_append_children(root, temp);
        free(temp->children);
        free(temp);
    }

    wordfree(&we);

    return root;
}
#else
static config_item_t *cf_read_generic(const char *path, const char *pattern, int depth)
{
    if (depth >= CF_MAX_DEPTH) {
        ERROR("configfile: Not including '%s' because the maximum "
              "nesting depth has been reached.", path);
        return NULL;
    }

    struct stat statbuf;
    int status = stat(path, &statbuf);
    if (status != 0) {
        ERROR("configfile: stat (%s) failed: %s", path, STRERRNO);
        return NULL;
    }

    if (S_ISREG(statbuf.st_mode))
        return cf_read_file(path, pattern, depth);
    else if (S_ISDIR(statbuf.st_mode))
        return cf_read_dir(path, pattern, depth);

    ERROR("configfile: %s is neither a file nor a directory.", path);
    return NULL;
}
#endif

int global_option_set(const char *option, const char *value, bool from_cli)
{
    DEBUG("option = %s; value = %s;", option, value);

    int i;
    for (i = 0; i < cf_global_options_num; i++)
        if (strcasecmp(cf_global_options[i].key, option) == 0)
            break;

    if (i >= cf_global_options_num) {
        ERROR("configfile: Cannot set unknown global option '%s'.", option);
        return -1;
    }

    if (cf_global_options[i].from_cli && (!from_cli)) {
        DEBUG("configfile: Ignoring %s '%s' option because "
              "it was overridden by a command-line option.", option, value);
        return 0;
    }

    free(cf_global_options[i].value);

    if (value != NULL)
        cf_global_options[i].value = strdup(value);
    else
        cf_global_options[i].value = NULL;

    cf_global_options[i].from_cli = from_cli;

    return 0;
}

const char *global_option_get(const char *option)
{
    int i;
    for (i = 0; i < cf_global_options_num; i++)
        if (strcasecmp(cf_global_options[i].key, option) == 0)
            break;

    if (i >= cf_global_options_num) {
        ERROR("configfile: Cannot get unknown global option '%s'.", option);
        return NULL;
    }

    return (cf_global_options[i].value != NULL) ? cf_global_options[i].value
                                                : cf_global_options[i].def;
}

long global_option_get_long(const char *option, long default_value)
{
    const char *str = global_option_get(option);
    if (NULL == str)
        return default_value;

    errno = 0;
    long value = strtol(str, /* endptr = */ NULL, /* base = */ 0);
    if (errno != 0)
        return default_value;

    return value;
}

cdtime_t global_option_get_time(const char *name, cdtime_t def)
{
    char const *optstr = global_option_get(name);
    if (optstr == NULL)
        return def;

    errno = 0;
    char *endptr = NULL;
    double v = strtod(optstr, &endptr);
    if ((endptr == NULL) || (*endptr != 0) || (errno != 0))
        return def;
    else if (v <= 0.0)
        return def;

    return DOUBLE_TO_CDTIME_T(v);
}

int global_option_get_cpumap(const char *name)
{
    if (cf_cpumap == NULL)
        return -1;

    for(int i=0; i < cf_cpumap_num; i++) {
        if (strcasecmp(cf_cpumap[i].name, name) == 0)
            return cf_cpumap[i].num;
    }

    return -1;
}

cdtime_t cf_get_default_interval(void)
{
    return global_option_get_time("interval", DOUBLE_TO_CDTIME_T(NCOLLECTD_DEFAULT_INTERVAL));
}

void cf_unregister_all(void)
{
    cf_callback_t *this = callback_head;
    while (this != NULL) {
        cf_callback_t *next = this->next;
        free(this->type);
        free(this);
        this = next;
    }
    callback_head = NULL;
}

void cf_unregister(const char *type)
{
    for (cf_callback_t *prev = NULL, *this = callback_head;
             this != NULL; prev = this, this = this->next)
        if (strcasecmp(this->type, type) == 0) {
            if (prev == NULL)
                callback_head = this->next;
            else
                prev->next = this->next;

            free(this->type);
            free(this);
            break;
        }
}

int cf_register(const char *type, int (*callback)(config_item_t *))
{
    cf_callback_t *new = malloc(sizeof(*new));
    if (new == NULL)
        return -1;

    new->type = strdup(type);
    if (new->type == NULL) {
        free(new);
        return -1;
    }

    new->callback = callback;
    new->next = NULL;

    new->ctx = plugin_get_ctx();

    if (callback_head == NULL) {
        callback_head = new;
    } else {
        cf_callback_t *last = callback_head;
        while (last->next != NULL)
            last = last->next;
        last->next = new;
    }

    return 0;
}

int cf_read(const char *filename, bool dump)
{
    config_item_t * conf = cf_read_generic(filename, /* pattern = */ NULL, /* depth = */ 0);
    if (conf == NULL) {
        ERROR("Unable to read config file %s.", filename);
        return -1;
    } else if (conf->children_num == 0) {
        ERROR("Configuration file %s is empty.", filename);
        config_free(conf);
        return -1;
    }

    int status = 0;
    for (int i = 0; i < conf->children_num; i++) {
        if (conf->children[i].children == NULL)
            status = dispatch_value(conf->children + i);
        else
            status = dispatch_block(conf->children + i);

        if (status != 0)
            break;
    }

    if (dump)
        config_dump(stdout, conf);

    config_free(conf);

    return status;
}

void global_options_free (void)
{
    for (int i = 0; i < cf_global_options_num; i++) {
        free(cf_global_options[i].value);
    }

    free(hostname_g);

    label_set_reset(&labels_g);

    for(int i = 0; i < cf_cpumap_num; i++) {
        free(cf_cpumap[i].name);
    }
    free(cf_cpumap);
}
