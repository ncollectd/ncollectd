// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

static char *path_sys_dmi;

static metric_family_t fam_dmi = {
    .name = "dmi",
    .type = METRIC_TYPE_INFO,
    .help = "Dmi info of bios, board, chassis and product.",
};

typedef struct {
    char *label;
    char *filename;
} dmi_files_t;

static dmi_files_t dmi_files[] = {
    {"bios_date",        "bios_date"        },
    {"bios_release",     "bios_release"     },
    {"bios_vendor",      "bios_vendor"      },
    {"bios_version",     "bios_version"     },
    {"board_asset_tag",  "board_asset_tag"  },
    {"board_name",       "board_name"       },
    {"board_serial",     "board_serial"     },
    {"board_vendor",     "board_vendor"     },
    {"board_version",    "board_version"    },
    {"chassis_asset_tag","chassis_asset_tag"},
    {"chassis_serial",   "chassis_serial"   },
    {"chassis_type",     "chassis_type"     },
    {"chassis_vendor",   "chassis_vendor"   },
    {"chassis_version",  "chassis_version"  },
    {"product_family",   "product_family"   },
    {"product_name",     "product_name"     },
    {"product_serial",   "product_serial"   },
    {"product_sku",      "product_sku"      },
    {"product_uuid",     "product_uuid"     },
    {"product_version",  "product_version"  },
    {"system_vendor",    "sys_vendor"       },
};

static int dmi_read(void)
{
    label_set_t info = {0};

    int dir_fd = open(path_sys_dmi, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) {
        PLUGIN_ERROR("Cannot open '%s': %s", path_sys_dmi, STRERRNO);
        return -1;
    }

    for (size_t i=0 ; i < STATIC_ARRAY_SIZE(dmi_files); i++) {
        char buffer[512];
        ssize_t len = read_file_at(dir_fd, dmi_files[i].filename, buffer, sizeof(buffer));
        if (len < 0)
            continue;
        char *line = strntrim(buffer, len);
        label_set_add(&info, true, dmi_files[i].label, line);
    }

    if (info.num > 0) {
        metric_family_append(&fam_dmi, VALUE_INFO(info), NULL, NULL);
        plugin_dispatch_metric_family(&fam_dmi, 0);
    }

    label_set_reset(&info);

    close(dir_fd);
    return 0;
}

static int dmi_init(void)
{
    path_sys_dmi = plugin_syspath("class/dmi/id");
    if (path_sys_dmi == NULL) {
        PLUGIN_ERROR("Cannot get sys path.");
        return -1;
    }

    return 0;
}

static int dmi_shutdown(void)
{
    free(path_sys_dmi);
    return 0;
}

void module_register(void)
{
    plugin_register_init("dmi", dmi_init);
    plugin_register_read("dmi", dmi_read);
    plugin_register_shutdown("dmi", dmi_shutdown);
}
