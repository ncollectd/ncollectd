// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#ifndef KERNEL_LINUX
#error "No applicable input method."
#endif

static char *path_proc_locks;

enum {
    FAM_LOCK,
    FAM_LOCK_PENDING,
    FAM_LOCK_MAX,
};

enum {
    LOCK_CLASS_POSIX,
    LOCK_CLASS_FLOCK,
    LOCK_CLASS_LEASE,
    LOCK_CLASS_MAX,
};

char *lock_class_name[LOCK_CLASS_MAX] = {
    [LOCK_CLASS_POSIX] = "POSIX",
    [LOCK_CLASS_FLOCK] = "FLOCK",
    [LOCK_CLASS_LEASE] = "LEASE",
};

enum {
    LOCK_TYPE_READ,
    LOCK_TYPE_WRITE,
    LOCK_TYPE_MAX,
};

char *lock_type_name[LOCK_TYPE_MAX] = {
    [LOCK_TYPE_READ]  = "READ",
    [LOCK_TYPE_WRITE] = "WRITE",
};

static metric_family_t fams[FAM_LOCK_MAX] = {
    [FAM_LOCK] = {
        .name = "system_locks",
        .type = METRIC_TYPE_GAUGE,
        .help = "Files currently locked by the kernel.",
    },
    [FAM_LOCK_PENDING] = {
        .name = "system_locks_pending",
        .type = METRIC_TYPE_GAUGE,
        .help = "File locks waiting.",
    },
};

static int locks_read(void)
{
    FILE *fh = fopen(path_proc_locks, "r");
    if (fh == NULL ) {
        PLUGIN_WARNING("Cannot open '%s': %s", path_proc_locks, STRERRNO);
        return -1;
    }

    uint64_t counter[FAM_LOCK_MAX][LOCK_CLASS_MAX][LOCK_TYPE_MAX] = {0};

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[16];
        int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 5)
            continue;

        /*
         * 47: FLOCK    ADVISORY    WRITE 714044 00:25:9880 0 EOF
         * 47: -> FLOCK  ADVISORY  WRITE 714256 00:25:9880 0 EOF
         */
        size_t kind = FAM_LOCK;
        char *lock_class = fields[1];
        char *lock_type = fields[3];

        if ((fields[1][0] == '-') && (fields[1][1] == '>') && (fields[1][2] == '\0')) {
            lock_class = fields[2];
            lock_type = fields[4];
            kind = FAM_LOCK_PENDING;
        }

        size_t type;
        if (!strcmp(lock_class, "POSIX") ||
            !strcmp(lock_class, "ACCESS") || !strcmp(lock_class, "OFDLCK")) {
            type = LOCK_CLASS_POSIX;
        } else if (!strcmp(lock_class, "FLOCK")) {
            type = LOCK_CLASS_FLOCK;
        } else if (!strcmp(lock_class, "DELEG") || !strcmp(lock_class, "LEASE")) {
            type = LOCK_CLASS_LEASE;
        } else {
            continue;
        }

        if (!strcmp(lock_type, "READ")) {
             counter[kind][type][LOCK_TYPE_READ]++;
        } else if (!strcmp(lock_type, "WRITE")) {
             counter[kind][type][LOCK_TYPE_WRITE]++;
        } else if (!strcmp(lock_type, "RW")) {
             counter[kind][type][LOCK_TYPE_READ]++;
             counter[kind][type][LOCK_TYPE_WRITE]++;
        }
    }

    fclose(fh);

    for (size_t j = 0; j < LOCK_CLASS_MAX; j++) {
        for (size_t i = 0; i < FAM_LOCK_MAX; i++) {
            for (size_t k = 0; k < LOCK_TYPE_MAX; k++) {
                metric_family_append(&fams[i], VALUE_GAUGE(counter[i][j][k]), NULL,
                                     &(label_pair_const_t){.name="class", .value=lock_class_name[j]},
                                     &(label_pair_const_t){.name="type", .value=lock_type_name[k]},
                                     NULL);
            }
        }
    }

    plugin_dispatch_metric_family_array(fams, FAM_LOCK_MAX, 0);
    return 0;
}

static int locks_init(void)
{
    path_proc_locks = plugin_procpath("locks");
    if (path_proc_locks == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int locks_shutdown(void)
{
    free(path_proc_locks);
    return 0;
}

void module_register(void)
{
    plugin_register_init("locks", locks_init);
    plugin_register_read("locks", locks_read);
    plugin_register_shutdown("locks", locks_shutdown);
}
