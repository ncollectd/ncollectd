// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2008 Alessandro Iurlano
// SPDX-FileCopyrightText: Copyright (C) 2008 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Alessandro Iurlano <alessandro.iurlano at gmail.com>
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libexpr/expr.h"

#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#define FC_RECURSIVE 1
#define FC_HIDDEN 2
#define FC_REGULAR 4

typedef struct {
    expr_value_t links;
    expr_value_t type;
    expr_value_t mode;
    expr_value_t path;
    expr_value_t name;
    expr_value_t inode;
    expr_value_t size;
    expr_value_t uid;
    expr_value_t gid;
    expr_value_t atime;
    expr_value_t mtime;
    expr_value_t ctime;
    expr_value_t now;
    expr_value_t minor;
    expr_value_t major;
} stat_values_t;

typedef struct {
    char *path;

    char *metric_files_size;
    char *help_files_size;
    char *metric_files_count;
    char *help_files_count;
    label_set_t labels;

    int options;

    /* Data counters */
    uint64_t files_num;
    uint64_t files_size;

    /* Selectors */
    char *name;
    int64_t mtime;
    int64_t size;

    stat_values_t stat_values;
    expr_symtab_t *symtab;
    expr_node_t *expr;

    /* Helper for the recursive functions */
    double tnow;
    time_t now;
} fc_directory_conf_t;

static fc_directory_conf_t **directories;
static size_t directories_num;

static void fc_free_dir(fc_directory_conf_t *dir)
{
    free(dir->path);
    free(dir->metric_files_size);
    free(dir->help_files_size);
    free(dir->metric_files_count);
    free(dir->help_files_count);
    free(dir->name);

    label_set_reset(&dir->labels);

    expr_symtab_free(dir->symtab);
    expr_node_free(dir->expr);

    free(dir);
}

static void fc_submit_dir(char *name, char *help, label_set_t *labels, double value)
{
    metric_family_t fam = {
            .name = name,
            .help = help,
            .type = METRIC_TYPE_GAUGE,
    };
    metric_family_append(&fam, VALUE_GAUGE(value), labels, NULL);
    plugin_dispatch_metric_family(&fam, 0);
}

static int fc_read_dir_callback(__attribute__((unused)) int dirfd, const char *dirname,
                                const char *filename, void *user_data)
{
    fc_directory_conf_t *dir = user_data;
    char abs_path[PATH_MAX];
    struct stat statbuf;

    if (dir == NULL)
        return -1;

    snprintf(abs_path, sizeof(abs_path), "%s/%s", dirname, filename);

    int status = lstat(abs_path, &statbuf);
    if (status != 0) {
        PLUGIN_ERROR("stat (%s) failed.", abs_path);
        return -1;
    }

    if (S_ISDIR(statbuf.st_mode) && (dir->options & FC_RECURSIVE)) {
        status = walk_directory(abs_path, fc_read_dir_callback, dir,
                                (dir->options & FC_HIDDEN) ? 1 : 0);
        return status;
    } else if ((dir->options & FC_REGULAR) && !S_ISREG(statbuf.st_mode)) {
        return 0;
    }

    if (dir->expr != NULL) {
        switch (statbuf.st_mode & S_IFMT) {
        case S_IFBLK:
            dir->stat_values.type.string = "b";
            break;
        case S_IFCHR:
            dir->stat_values.type.string = "c";
            break;
        case S_IFDIR:
            dir->stat_values.type.string = "d";
            break;
        case S_IFIFO:
            dir->stat_values.type.string = "p";
            break;
        case S_IFLNK:
            dir->stat_values.type.string = "l";
            break;
        case S_IFREG:
            dir->stat_values.type.string = "f";
            break;
        case S_IFSOCK:
            dir->stat_values.type.string = "s";
            break;
        default:
            dir->stat_values.type.string = "?";
            break;
        }

        dir->stat_values.path.string = abs_path;
        dir->stat_values.name.string = discard_const(filename);
        dir->stat_values.links.number = statbuf.st_nlink;
        dir->stat_values.mode.number = statbuf.st_mode & 07777;
        dir->stat_values.inode.number = statbuf.st_ino;
        dir->stat_values.size.number = statbuf.st_size;
        dir->stat_values.uid.number = statbuf.st_uid;
        dir->stat_values.gid.number = statbuf.st_gid;

#ifdef KERNEL_DARWIN
        dir->stat_values.atime.number = TIMESPEC_TO_DOUBLE(&statbuf.st_atimespec);
        dir->stat_values.mtime.number = TIMESPEC_TO_DOUBLE(&statbuf.st_mtimespec);
        dir->stat_values.ctime.number = TIMESPEC_TO_DOUBLE(&statbuf.st_ctimespec);
#else
        dir->stat_values.atime.number = TIMESPEC_TO_DOUBLE(&statbuf.st_atim);
        dir->stat_values.mtime.number = TIMESPEC_TO_DOUBLE(&statbuf.st_mtim);
        dir->stat_values.ctime.number = TIMESPEC_TO_DOUBLE(&statbuf.st_ctim);
#endif

        dir->stat_values.now.number = dir->tnow;
        dir->stat_values.minor.number =  minor(statbuf.st_dev);
        dir->stat_values.major.number =  major(statbuf.st_dev);

        expr_value_t *value = expr_eval(dir->expr);
        if (expr_value_to_bool(value) == false)
            return 0;
    }

    if (dir->name != NULL) {
        status = fnmatch(dir->name, filename, /* flags = */ 0);
        if (status != 0)
            return 0;
    }

    if (!S_ISREG(statbuf.st_mode)) {
        dir->files_num++;
        return 0;
    }

    if (dir->mtime != 0) {
        time_t mtime = dir->now;

        if (dir->mtime < 0)
            mtime += dir->mtime;
        else
            mtime -= dir->mtime;

        PLUGIN_DEBUG("Only collecting files that were touched %s %u.",
                     (dir->mtime < 0) ? "after" : "before", (unsigned int)mtime);

        if (((dir->mtime < 0) && (statbuf.st_mtime < mtime)) ||
            ((dir->mtime > 0) && (statbuf.st_mtime > mtime)))
            return 0;
    }

    if (dir->size != 0) {
        off_t size;

        if (dir->size < 0)
            size = (off_t)((-1) * dir->size);
        else
            size = (off_t)dir->size;

        if (((dir->size < 0) && (statbuf.st_size > size)) ||
            ((dir->size > 0) && (statbuf.st_size < size)))
            return 0;
    }

    dir->files_num++;
    dir->files_size += (uint64_t)statbuf.st_size;

    return 0;
}

static int fc_read_dir(fc_directory_conf_t *dir)
{
    dir->files_num = 0;
    dir->files_size = 0;

    dir->tnow = CDTIME_T_TO_DOUBLE(cdtime());

    if (dir->mtime != 0)
        dir->now = time(NULL);

    int status = walk_directory(dir->path, fc_read_dir_callback, dir,
                                (dir->options & FC_HIDDEN) ? 1 : 0);
    if (status != 0) {
        PLUGIN_WARNING("walk_directory (%s) failed.", dir->path);
        return -1;
    }

    if (dir->metric_files_count != NULL)
        fc_submit_dir(dir->metric_files_count, dir->help_files_count,
                      &dir->labels, (double)dir->files_num);

    if (dir->metric_files_size != NULL)
        fc_submit_dir(dir->metric_files_size, dir->help_files_size,
                      &dir->labels, (double)dir->files_size);

    return 0;
}

static int fc_read(void)
{
    for (size_t i = 0; i < directories_num; i++)
        fc_read_dir(directories[i]);

    return 0;
}

/*
 * Config:
 * plugin filecount {
 *   directory "/path/to/dir" {
 *       metric-files-size "foo_bytes"
 *       metric-files-count "foo_files"
 *       name "*.conf"
 *       mtime -3600
 *       size "+10M"
 *       recursive true
 *       include-hidden false
 *   }
 * }
 *
 * Collect:
 * - Number of files
 * - Total size
 */

static int fc_config_add_dir_mtime(fc_directory_conf_t *dir, config_item_t *ci)
{
    if ((ci->values_num != 1) ||
        ((ci->values[0].type != CONFIG_TYPE_STRING) &&
        (ci->values[0].type != CONFIG_TYPE_NUMBER))) {
        PLUGIN_ERROR("The 'mtime' config option needs exactly one string or numeric argument.");
        return -1;
    }

    if (ci->values[0].type == CONFIG_TYPE_NUMBER) {
        dir->mtime = (int64_t)ci->values[0].value.number;
        return 0;
    }

    errno = 0;
    char *endptr = NULL;
    double temp = strtod(ci->values[0].value.string, &endptr);
    if ((errno != 0) || (endptr == NULL) || (endptr == ci->values[0].value.string)) {
        PLUGIN_ERROR("Converting `%s' to a number failed.", ci->values[0].value.string);
        return -1;
    }

    switch (*endptr) {
    case 0:
    case 's':
    case 'S':
        break;

    case 'm':
    case 'M':
        temp *= 60;
        break;

    case 'h':
    case 'H':
        temp *= 3600;
        break;

    case 'd':
    case 'D':
        temp *= 86400;
        break;

    case 'w':
    case 'W':
        temp *= 7 * 86400;
        break;

    case 'y':
    case 'Y':
        temp *= 31557600; /* == 365.25 * 86400 */
        break;

    default:
        PLUGIN_ERROR("Invalid suffix for 'mtime': `%c'", *endptr);
        return -1;
    }

    dir->mtime = (int64_t)temp;

    return 0;
}

static int fc_config_add_dir_size(fc_directory_conf_t *dir, config_item_t *ci)
{
    if ((ci->values_num != 1) ||
        ((ci->values[0].type != CONFIG_TYPE_STRING) &&
        (ci->values[0].type != CONFIG_TYPE_NUMBER))) {
        PLUGIN_ERROR("The 'size' config option needs exactly one string or numeric argument.");
        return -1;
    }

    if (ci->values[0].type == CONFIG_TYPE_NUMBER) {
        dir->size = (int64_t)ci->values[0].value.number;
        return 0;
    }

    errno = 0;
    char *endptr = NULL;
    double temp = strtod(ci->values[0].value.string, &endptr);
    if ((errno != 0) || (endptr == NULL) || (endptr == ci->values[0].value.string)) {
        PLUGIN_ERROR("Converting '%s' to a number failed.", ci->values[0].value.string);
        return -1;
    }

    switch (*endptr) {
    case 0:
    case 'b':
    case 'B':
        break;

    case 'k':
    case 'K':
        temp *= 1000.0;
        break;

    case 'm':
    case 'M':
        temp *= 1000.0 * 1000.0;
        break;

    case 'g':
    case 'G':
        temp *= 1000.0 * 1000.0 * 1000.0;
        break;

    case 't':
    case 'T':
        temp *= 1000.0 * 1000.0 * 1000.0 * 1000.0;
        break;

    case 'p':
    case 'P':
        temp *= 1000.0 * 1000.0 * 1000.0 * 1000.0 * 1000.0;
        break;

    default:
        PLUGIN_ERROR("Invalid suffix for 'size': `%c'", *endptr);
        return -1;
    }

    dir->size = (int64_t)temp;

    return 0;
}



static int fc_config_add_dir_option(fc_directory_conf_t *dir, config_item_t *ci, int bit)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_BOOLEAN)) {
        PLUGIN_WARNING("The 'recursive' config options needs exactly one boolean argument.");
        return -1;
    }

    if (ci->values[0].value.boolean)
        dir->options |= bit;
    else
        dir->options &= ~bit;

    return 0;
}

static int fc_config_add_expr(fc_directory_conf_t *dir, config_item_t *ci)
{
    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    dir->symtab = expr_symtab_alloc();
    if (dir->symtab == NULL)
        return -1;

    dir->stat_values.links.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "links", &dir->stat_values.links);
    dir->stat_values.type.type = EXPR_VALUE_STRING;
    expr_symtab_append_name_value(dir->symtab, "type", &dir->stat_values.type);
    dir->stat_values.mode.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "mode", &dir->stat_values.mode);
    dir->stat_values.path.type = EXPR_VALUE_STRING;
    expr_symtab_append_name_value(dir->symtab, "path", &dir->stat_values.path);
    dir->stat_values.name.type = EXPR_VALUE_STRING;
    expr_symtab_append_name_value(dir->symtab, "name", &dir->stat_values.name);
    dir->stat_values.inode.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "inode", &dir->stat_values.inode);
    dir->stat_values.size.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "size", &dir->stat_values.size);
    dir->stat_values.uid.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "uid", &dir->stat_values.uid);
    dir->stat_values.gid.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "gid", &dir->stat_values.gid);
    dir->stat_values.atime.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "atime", &dir->stat_values.atime);
    dir->stat_values.mtime.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "mtime", &dir->stat_values.mtime);
    dir->stat_values.ctime.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "ctime", &dir->stat_values.ctime);
    dir->stat_values.now.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "now", &dir->stat_values.now);
    dir->stat_values.minor.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "minor", &dir->stat_values.minor);
    dir->stat_values.major.type = EXPR_VALUE_NUMBER;
    expr_symtab_append_name_value(dir->symtab, "major", &dir->stat_values.major);

    dir->expr = expr_parse(ci->values[0].value.string, dir->symtab);
    if (dir->expr == NULL)
        return -1;

    return 0;
}

static int fc_config_add_dir(config_item_t *ci)
{
    /* Initialize `dir' */
    fc_directory_conf_t *dir = calloc(1, sizeof(*dir));
    if (dir == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &dir->path);
    if (status != 0) {
        PLUGIN_ERROR("Missing directory name.");
        fc_free_dir(dir);
        return -1;
    }

    dir->options = FC_RECURSIVE | FC_REGULAR;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *option = ci->children + i;

        if (strcasecmp("metric-files-size", option->key) == 0) {
            status = cf_util_get_string(option, &dir->metric_files_size);
        } else if (strcasecmp("help-files-size", option->key) == 0) {
            status = cf_util_get_string(option, &dir->help_files_size);
        } else if (strcasecmp("metric-files-count", option->key) == 0) {
            status = cf_util_get_string(option, &dir->metric_files_count);
        } else if (strcasecmp("help-files-count", option->key) == 0) {
            status = cf_util_get_string(option, &dir->help_files_count);
        } else if (strcasecmp("label", option->key) == 0) {
            status = cf_util_get_label(option, &dir->labels);
        } else if (strcasecmp("expr", option->key) == 0) {
            status = fc_config_add_expr(dir, option);
        } else if (strcasecmp("name", option->key) == 0) {
            status = cf_util_get_string(option, &dir->name);
        } else if (strcasecmp("mtime", option->key) == 0) {
            status = fc_config_add_dir_mtime(dir, option);
        } else if (strcasecmp("size", option->key) == 0) {
            status = fc_config_add_dir_size(dir, option);
        } else if (strcasecmp("recursive", option->key) == 0) {
            status = fc_config_add_dir_option(dir, option, FC_RECURSIVE);
        } else if (strcasecmp("include-hidden", option->key) == 0) {
            status = fc_config_add_dir_option(dir, option, FC_HIDDEN);
        } else if (strcasecmp("regular-only", option->key) == 0) {
            status = fc_config_add_dir_option(dir, option, FC_REGULAR);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         option->key, cf_get_file(option), cf_get_lineno(option));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        fc_free_dir(dir);
        return -1;
    }

    if (dir->metric_files_size == NULL && dir->metric_files_count == NULL) {
        PLUGIN_WARNING("Both 'metric-files-size' and 'metric-files-count' "
                       "are disabled for '%s'. There's no metric to report.", dir->path);
        fc_free_dir(dir);
        return -1;
    }

    /* Ready to add it to list */
    fc_directory_conf_t **temp = realloc(directories, sizeof(*directories) * (directories_num + 1));
    if (temp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        fc_free_dir(dir);
        return -1;
    }

    directories = temp;
    directories[directories_num] = dir;
    directories_num++;

    return 0;
}

static int fc_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("directory", child->key) == 0) {
            status = fc_config_add_dir(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int fc_init(void)
{
    if (directories_num < 1) {
        PLUGIN_WARNING("No directories have been configured.");
        return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("filecount", fc_config);
    plugin_register_init("filecount", fc_init);
    plugin_register_read("filecount", fc_read);
}
