// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

enum {
    FAM_NF_CONNTRACK_ENTRIES,
    FAM_NF_CONNTRACK_SEARCHED,
    FAM_NF_CONNTRACK_FOUND,
    FAM_NF_CONNTRACK_NEW,
    FAM_NF_CONNTRACK_INVALID,
    FAM_NF_CONNTRACK_IGNORE,
    FAM_NF_CONNTRACK_DELETE,
    FAM_NF_CONNTRACK_DELETE_LIST,
    FAM_NF_CONNTRACK_INSERT,
    FAM_NF_CONNTRACK_INSERT_FAILED,
    FAM_NF_CONNTRACK_DROP,
    FAM_NF_CONNTRACK_EARLY_DROP,
    FAM_NF_CONNTRACK_ICMP_ERROR,
    FAM_NF_CONNTRACK_EXPECT_NEW,
    FAM_NF_CONNTRACK_EXPECT_CREATE,
    FAM_NF_CONNTRACK_EXPECT_DELETE,
    FAM_NF_CONNTRACK_SEARCH_RESTART,
    FAM_NF_CONNTRACK_MAX
};

static metric_family_t fams_nf_conntrack[FAM_NF_CONNTRACK_MAX] = {
    [FAM_NF_CONNTRACK_ENTRIES] = {
        .name = "system_nf_conntrack_entries",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries in conntrack table.",
    },
    [FAM_NF_CONNTRACK_SEARCHED] = {
        .name = "system_nf_conntrack_searched",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of conntrack table lookups performed.",
    },
    [FAM_NF_CONNTRACK_FOUND] = {
        .name = "system_nf_conntrack_found",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of searched entries which were successful.",
    },
    [FAM_NF_CONNTRACK_NEW] = {
        .name = "system_nf_conntrack_new",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of conntrack entries added which were not expected before.",
    },
    [FAM_NF_CONNTRACK_INVALID] = {
        .name = "system_nf_conntrack_invalid",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets seen which can not be tracked.",
    },
    [FAM_NF_CONNTRACK_IGNORE] = {
        .name = "system_nf_conntrack_ignore",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets seen which are already connected to a conntrack entry.",
    },
    [FAM_NF_CONNTRACK_DELETE] = {
        .name = "system_nf_conntrack_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of conntrack entries which were removed.",
    },
    [FAM_NF_CONNTRACK_DELETE_LIST] = {
        .name = "system_nf_conntrack_delete_list",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of conntrack entries which were put to dying list.",
    },
    [FAM_NF_CONNTRACK_INSERT] = {
        .name = "system_nf_conntrack_insert",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries inserted into the list.",
    },
    [FAM_NF_CONNTRACK_INSERT_FAILED] = {
        .name = "system_nf_conntrack_insert_failed",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of entries for which list insertion was attempted but failed "
                "(happens if the same entry is already present).",
    },
    [FAM_NF_CONNTRACK_DROP] = {
        .name = "system_nf_conntrack_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets dropped due to conntrack failure. "
                "Either new conntrack entry allocation failed, "
                "or protocol helper dropped the packet.",
    },
    [FAM_NF_CONNTRACK_EARLY_DROP] = {
        .name = "system_nf_conntrack_early_drop",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of dropped conntrack entries to make room for new ones, "
                "if maximum table size was reached.",
    },
    [FAM_NF_CONNTRACK_ICMP_ERROR] = {
        .name = "system_nf_conntrack_icmp_error",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of packets which could not be tracked due to error situation. "
                "This is a subset of invalid.",
    },
    [FAM_NF_CONNTRACK_EXPECT_NEW] = {
        .name = "system_nf_conntrack_expect_new",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of conntrack entries added after an expectation "
                "for them was already present.",
    },
    [FAM_NF_CONNTRACK_EXPECT_CREATE] = {
        .name = "system_nf_conntrack_expect_create",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of expectations added.",
    },
    [FAM_NF_CONNTRACK_EXPECT_DELETE] = {
        .name = "system_nf_conntrack_expect_delete",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of expectations deleted.",
    },
    [FAM_NF_CONNTRACK_SEARCH_RESTART] = {
        .name = "system_nf_conntrack_search_restart",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of conntrack table lookups which had to be restarted "
                "due to hashtable resizes.",
    },
};

typedef struct {
    int field;
    int fam;
} field_fam_t;

static field_fam_t field_fam[] = {
    { 0,  FAM_NF_CONNTRACK_ENTRIES        },
    { 1,  FAM_NF_CONNTRACK_SEARCHED       },
    { 2,  FAM_NF_CONNTRACK_FOUND          },
    { 3,  FAM_NF_CONNTRACK_NEW            },
    { 4,  FAM_NF_CONNTRACK_INVALID        },
    { 5,  FAM_NF_CONNTRACK_IGNORE         },
    { 6,  FAM_NF_CONNTRACK_DELETE         },
    { 7,  FAM_NF_CONNTRACK_DELETE_LIST    },
    { 8,  FAM_NF_CONNTRACK_INSERT         },
    { 9,  FAM_NF_CONNTRACK_INSERT_FAILED  },
    { 10, FAM_NF_CONNTRACK_DROP           },
    { 11, FAM_NF_CONNTRACK_EARLY_DROP     },
    { 12, FAM_NF_CONNTRACK_ICMP_ERROR     },
    { 13, FAM_NF_CONNTRACK_EXPECT_NEW     },
    { 14, FAM_NF_CONNTRACK_EXPECT_CREATE  },
    { 15, FAM_NF_CONNTRACK_EXPECT_DELETE  },
    { 16, FAM_NF_CONNTRACK_SEARCH_RESTART }
};
static size_t field_fam_size = STATIC_ARRAY_SIZE(fams_nf_conntrack);

static char *path_proc_nf_conntrack;

static int nf_conntrack_read(void)
{
    FILE *fh = fopen(path_proc_nf_conntrack, "r");
    if (fh == NULL) {
        PLUGIN_ERROR("Unable to open '%s': %s", path_proc_nf_conntrack, STRERRNO);
        return -1;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fh) == NULL) {
        fclose(fh);
        PLUGIN_WARNING("Unable to read  %s", path_proc_nf_conntrack);
        return -1;
    }

    char *fields[17];
    for (int ncpu = 0; fgets(buffer, sizeof(buffer), fh) != NULL ; ncpu++) {
        int fields_num = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (fields_num < 17)
            continue;

        char cpu[64];
        ssnprintf(cpu, sizeof(cpu), "%d", ncpu);

        for (size_t i = 0; i < field_fam_size; i++) {
            uint64_t value = (uint64_t)strtoull(fields[field_fam[i].field], NULL, 16);
            metric_family_append(&fams_nf_conntrack[field_fam[i].fam], VALUE_COUNTER(value), NULL,
                                 &(label_pair_const_t){.name="cpu", .value=cpu}, NULL);
        }
    }
    fclose(fh);

    plugin_dispatch_metric_family_array(fams_nf_conntrack, FAM_NF_CONNTRACK_MAX, 0);
    return 0;
}

static int nf_conntrack_init(void)
{
    path_proc_nf_conntrack = plugin_procpath("net/stat/nf_conntrack");
    if (path_proc_nf_conntrack == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    return 0;
}

static int nf_conntrack_shutdown(void)
{
    free(path_proc_nf_conntrack);
    return 0;
}

void module_register(void)
{
    plugin_register_init("nfconntrack", nf_conntrack_init);
    plugin_register_read("nfconntrack", nf_conntrack_read);
    plugin_register_shutdown("nfconntrack", nf_conntrack_shutdown);
}
