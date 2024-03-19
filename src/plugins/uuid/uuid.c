// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007 Red Hat Inc.
// SPDX-FileCopyrightText: Copyright (C) 2015 Ruben Kerkhof
// SPDX-FileContributor: Dan Berrange <berrange at redhat.com>
// SPDX-FileContributor: Richard W.M. Jones <rjones at redhat.com>

// Derived from UUID detection code by Dan Berrange <berrange@redhat.com>
// http://hg.et.redhat.com/virt/daemons/spectre--devel?f=f6e3a1b06433;file=lib/uuid.c

#include "plugin.h"
#include "libutils/common.h"

#if (defined(HAVE_SYS_SYSCTL_H) && defined(HAVE_SYSCTLBYNAME)) || defined(KERNEL_OPENBSD)
/* Implies have BSD variant */
#    include <sys/sysctl.h>
#endif

#define UUID_RAW_LENGTH 16
#define UUID_PRINTABLE_COMPACT_LENGTH (UUID_RAW_LENGTH * 2)
#define UUID_PRINTABLE_NORMAL_LENGTH (UUID_PRINTABLE_COMPACT_LENGTH + 4)

static char *uuidfile;

#if defined(KERNEL_DARWIN) || defined(KERNEL_FREEBSD) || defined(KERNEL_NETBSD)
static char *uuid_get_from_sysctlbyname(const char *name)
{
    char uuid[UUID_PRINTABLE_NORMAL_LENGTH + 1];
    size_t len = sizeof(uuid);
    if (sysctlbyname(name, &uuid, &len, NULL, 0) == -1)
        return NULL;
    return strdup(uuid);
}
#elif defined(KERNEL_OPENBSD)
static char *uuid_get_from_sysctl(void)
{
    char uuid[UUID_PRINTABLE_NORMAL_LENGTH + 1];
    size_t len = sizeof(uuid);
    int mib[2] = { CTL_HW, HW_UUID};

    if (sysctl(mib, 2, uuid, &len, NULL, 0) == -1)
        return NULL;
    return strdup(uuid);
}
#endif

static char *uuid_get_from_file(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
        return NULL;

    char uuid[UUID_PRINTABLE_NORMAL_LENGTH + 1] = "";
    if (!fgets(uuid, sizeof(uuid), file)) {
        fclose(file);
        return NULL;
    }
    fclose(file);
    strstripnewline(uuid);

    return strdup(uuid);
}

#if defined(KERNEL_LINUX)
static char *uuid_get_from_sys_file(const char *path)
{
    char *path_sys = plugin_syspath(path);
    if (path_sys == NULL)
        return NULL;

    char *uuid =  uuid_get_from_file(path_sys);

    free(path_sys);

    return uuid;
}
#endif

static char *uuid_get_local(void)
{
    char *uuid;

    /* Check /etc/uuid / UUIDFile before any other method. */
    if ((uuid = uuid_get_from_file(uuidfile ? uuidfile : "/etc/uuid")) != NULL)
        return uuid;

#if defined(KERNEL_DARWIN)
    if ((uuid = uuid_get_from_sysctlbyname("kern.uuid")) != NULL)
        return uuid;
#elif defined(KERNEL_FREEBSD)
    if ((uuid = uuid_get_from_sysctlbyname("kern.hostuuid")) != NULL)
        return uuid;
#elif defined(KERNEL_NETBSD)
    if ((uuid = uuid_get_from_sysctlbyname("machdep.dmi.system-uuid")) != NULL)
        return uuid;
#elif defined(KERNEL_OPENBSD)
    if ((uuid = uuid_get_from_sysctl()) != NULL)
        return uuid;
#elif defined(KERNEL_LINUX)
    if ((uuid = uuid_get_from_sys_file("class/dmi/id/product_uuid")) != NULL)
        return uuid;
    if ((uuid = uuid_get_from_sys_file("hypervisor/uuid")) != NULL)
        return uuid;
#endif

    return NULL;
}

static int uuid_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp(child->key, "uuid-file") == 0) {
            status = cf_util_get_string(child, &uuidfile);
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

static int uuid_init(void)
{
    char *uuid = uuid_get_local();

    if (uuid) {
        plugin_set_hostname(uuid);
        free(uuid);
        return 0;
    }

    PLUGIN_WARNING("could not read UUID using any known method");
    return 0;
}

void module_register(void)
{
    plugin_register_config("uuid", uuid_config);
    plugin_register_init("uuid", uuid_init);
}
