// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2010,2011 Michael Hanselmann
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Michael Hanselmann
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/exclist.h"

#include <sys/ioctl.h>

#include <linux/major.h>
#include <linux/raid/md_u.h>

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

static char *path_proc_diskstats;

#define DEV_DIR "/dev"

static exclist_t excl_device;

enum {
    FAM_MD_ACTIVE,
    FAM_MD_FAILED,
    FAM_MD_SPARE,
    FAM_MD_MISSING,
    FAM_MD_MAX,
};

static metric_family_t fams[FAM_MD_MAX] = {
    [FAM_MD_ACTIVE] = {
        .name = "system_md_active",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of active (in sync) disks.",
    },
    [FAM_MD_FAILED] = {
        .name = "system_md_failed",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of failed disks.",
    },
    [FAM_MD_SPARE] = {
        .name = "system_md_spare",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of stand-by disks.",
    },
    [FAM_MD_MISSING] = {
        .name = "system_md_missing",
        .type = METRIC_TYPE_GAUGE,
        .help = "Number of missing disks.",
    },
};

static void md_process(const int minor, const char *path)
{
    int fd;
    struct stat st;
    mdu_array_info_t array;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        PLUGIN_WARNING("open(%s): %s", path, STRERRNO);
        return;
    }

    if (fstat(fd, &st) < 0) {
        PLUGIN_WARNING("Unable to fstat file descriptor for %s: %s", path, STRERRNO);
        close(fd);
        return;
    }

    if (!S_ISBLK(st.st_mode)) {
        PLUGIN_WARNING("%s is no block device", path);
        close(fd);
        return;
    }

    if (st.st_rdev != makedev(MD_MAJOR, minor)) {
        PLUGIN_WARNING("Major/minor of %s are %i:%i, should be %i:%i", path,
                       (int)major(st.st_rdev), (int)minor(st.st_rdev), (int)MD_MAJOR, minor);
        close(fd);
        return;
    }

    /* Retrieve md information */
    if (ioctl(fd, GET_ARRAY_INFO, &array) < 0) {
        PLUGIN_WARNING("Unable to retrieve array info from %s: %s", path, STRERRNO);
        close(fd);
        return;
    }

    close(fd);

    /*
     * The mdu_array_info_t structure contains numbers of disks in the array.
     * However, disks are accounted for more than once:
     *
     * active:  Number of active (in sync) disks.
     * spare:   Number of stand-by disks.
     * working: Number of working disks. (active + sync)
     * failed:  Number of failed disks.
     * nr:      Number of physically present disks. (working + failed)
     * raid:    Number of disks in the RAID. This may be larger than "nr" if
     *                  disks are missing and smaller than "nr" when spare disks are
     *                  around.
     */
    char minor_buffer[21];
    ssnprintf(minor_buffer, sizeof(minor_buffer), "%i", minor);

    metric_family_append(&fams[FAM_MD_ACTIVE], VALUE_GAUGE(array.active_disks), NULL,
                         &(label_pair_const_t){.name="device", .value=path},
                         &(label_pair_const_t){.name="minor", .value=minor_buffer}, NULL);

    metric_family_append(&fams[FAM_MD_FAILED], VALUE_GAUGE(array.failed_disks), NULL,
                         &(label_pair_const_t){.name="device", .value=path},
                         &(label_pair_const_t){.name="minor", .value=minor_buffer}, NULL);

    metric_family_append(&fams[FAM_MD_SPARE], VALUE_GAUGE(array.spare_disks), NULL,
                         &(label_pair_const_t){.name="device", .value=path},
                         &(label_pair_const_t){.name="minor", .value=minor_buffer}, NULL);

    double disks_missing = 0.0;
    if (array.raid_disks > array.nr_disks)
        disks_missing = (double)(array.raid_disks - array.nr_disks);

    metric_family_append(&fams[FAM_MD_MISSING], VALUE_GAUGE(disks_missing), NULL,
                         &(label_pair_const_t){.name="device", .value=path},
                         &(label_pair_const_t){.name="minor", .value=minor_buffer}, NULL);
}

static int md_read(void)
{
    FILE *fh = fopen(path_proc_diskstats, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("Unable to open %s: %s", path_proc_diskstats, STRERRNO);
        return -1;
    }

    /* Iterate md devices */
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char path[PATH_MAX];
        char *fields[4];
        char *name;
        int major, minor;

        /* Extract interesting fields */
        if (strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields)) < 3)
            continue;

        major = atoi(fields[0]);

        if (major != MD_MAJOR)
            continue;

        minor = atoi(fields[1]);
        name = fields[2];

        if (!exclist_match(&excl_device, name))
            continue;

        /* FIXME: Don't hardcode path. Walk /dev collecting major,
         * minor and name, then use lookup table to find device.
         * Alternatively create a temporary device file with correct
         * major/minor, but that again can be tricky if the filesystem
         * with the device file is mounted using the "nodev" option.
         */
        snprintf(path, sizeof(path), "%s/%s", DEV_DIR, name);

        md_process(minor, path);
    }

    fclose(fh);

    plugin_dispatch_metric_family_array(fams, FAM_MD_MAX, 0);
    return 0;
}

static int md_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "device") == 0) {
            status = cf_util_exclist(child, &excl_device);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int md_init(void)
{
    path_proc_diskstats = plugin_procpath("diskstats");
    if (path_proc_diskstats == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int md_shutdown(void)
{
    free(path_proc_diskstats);
    exclist_reset(&excl_device);
    return 0;
}

void module_register(void)
{
    plugin_register_init("md", md_init);
    plugin_register_config("md", md_config);
    plugin_register_read("md", md_read);
    plugin_register_shutdown("md", md_shutdown);
}
