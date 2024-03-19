// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2005-2014 Florian octo Forster
// SPDX-FileCopyrightText: Copyright (C) 2009 Stefan Völkel
// SPDX-FileCopyrightText: Copyright (C) 2009-2024 Manuel Sanmartín
// SPDX-FileCopyrightText: Copyright (C) 2010 Aurélien Reynaud
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
// SPDX-FileContributor: Aurélien Reynaud <collectd at wattapower.net>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#ifdef HAVE_CONFIG_H
#include "config.h"
#undef HAVE_CONFIG_H
#endif
/* avoid swap.h error "Cannot use swapctl in the large files compilation environment" */
#if defined(HAVE_SYS_SWAP_H) && !defined(_LP64) && _FILE_OFFSET_BITS == 64
#undef _FILE_OFFSET_BITS
#undef _LARGEFILE64_SOURCE
#endif

#include "plugin.h"
#include "libutils/common.h"

#ifdef HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif
#ifdef HAVE_VM_ANON_H
#include <vm/anon.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if (defined(HAVE_SYS_SYSCTL_H) && defined(HAVE_SYSCTLBYNAME)) || defined(__OpenBSD__)
/* implies BSD variant */
#include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_DKSTAT_H
#include <sys/dkstat.h>
#endif
#ifdef HAVE_KVM_H
#include <kvm.h>
#endif

#ifdef HAVE_PERFSTAT
#include <libperfstat.h>
#include <sys/protosw.h>
#endif

#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))


#ifdef KERNEL_LINUX
static char *path_proc_swaps;
static char *path_proc_meminfo;
static char *path_proc_vmstat;

#define SWAP_HAVE_REPORT_BY_DEVICE 1
static int64_t pagesize;
static bool report_by_device;

#elif defined(HAVE_SWAPCTL) && (defined(HAVE_SWAPCTL_TWO_ARGS) || defined(HAVE_SWAPCTL_THREE_ARGS))
#define SWAP_HAVE_REPORT_BY_DEVICE 1
static int64_t pagesize;
static bool report_by_device;

#elif defined(HAVE_SWAPCTL) && defined(HAVE_SWAPCTL_THREE_ARGS)
/* No global variables */

#elif defined(VM_SWAPUSAGE)
/* No global variables */

#elif defined(HAVE_LIBKVM_GETSWAPINFO)
static kvm_t *kvm_obj;
int kvm_pagesize;

#elif defined(HAVE_PERFSTAT)
static int pagesize;

#else
#error "No applicable input method."
#endif

enum {
    FAM_SWAP_USED_BYTES,
    FAM_SWAP_FREE_BYTES,
    FAM_SWAP_CACHED_BYTES,
    FAM_SWAP_RESERVED_BYTES,
    FAM_SWAP_IN_BYTES,
    FAM_SWAP_OUT_BYTES,
    FAM_SWAP_MAX,
};

static metric_family_t fams[FAM_SWAP_MAX] = {
    [FAM_SWAP_USED_BYTES] = {
        .name = "system_swap_used_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory which has been evicted from RAM, and is temporarily on the disk.",
    },
    [FAM_SWAP_FREE_BYTES] = {
        .name = "system_swap_free_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The remaining swap space available."
    },
    [FAM_SWAP_CACHED_BYTES] = {
        .name = "system_swap_cached_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Memory that once was swapped out, is swapped back in "
                "but still also is in the swapfile.",
    },
    [FAM_SWAP_RESERVED_BYTES] = {
        .name = "system_swap_reserved_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = NULL,
    },
    [FAM_SWAP_IN_BYTES] = {
        .name = "system_swap_in_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes the system has swapped in from disk.",
    },
    [FAM_SWAP_OUT_BYTES] = {
        .name = "system_swap_out_bytes",
        .type = METRIC_TYPE_COUNTER,
        .help = "Number of bytes the system has swapped out to disk.",
    },
};

#ifdef KERNEL_LINUX
static int swap_read_separate(void)
{
    FILE *fh = fopen(path_proc_swaps, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("Cannot open '%s': %s", path_proc_swaps, STRERRNO);
        return -1;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[8];
        int numfields;
        char *endptr;

        char path[PATH_MAX];
        double total;
        double used;

        numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields != 5)
            continue;

        sstrncpy(path, fields[0], sizeof(path));

        errno = 0;
        endptr = NULL;
        total = strtod(fields[2], &endptr);
        if ((endptr == fields[2]) || (errno != 0))
            continue;

        errno = 0;
        endptr = NULL;
        used = strtod(fields[3], &endptr);
        if ((endptr == fields[3]) || (errno != 0))
            continue;

        if (total < used)
            continue;

        metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(used * 1024.0), NULL,
                             &(label_pair_const_t){.name="device", .value=path}, NULL);
        metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE((total - used) * 1024.0), NULL,
                             &(label_pair_const_t){.name="device", .value=path}, NULL);
    }

    fclose(fh);

    return 0;
}

static int swap_read_combined(void)
{
    FILE *fh = fopen(path_proc_meminfo, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("Cannot open '%s': %s", path_proc_meminfo, STRERRNO);
        return -1;
    }

    double swap_used = NAN;
    double swap_cached = NAN;
    double swap_free = NAN;
    double swap_total = NAN;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[8];
        int numfields;

        numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));
        if (numfields < 2)
            continue;

        if (strcasecmp(fields[0], "SwapTotal:") == 0)
            strtodouble(fields[1], &swap_total);
        else if (strcasecmp(fields[0], "SwapFree:") == 0)
            strtodouble(fields[1], &swap_free);
        else if (strcasecmp(fields[0], "SwapCached:") == 0)
            strtodouble(fields[1], &swap_cached);
    }

    fclose(fh);

    if (isnan(swap_total) || isnan(swap_free))
        return ENOENT;

    /* Some systems, OpenVZ for example, don't provide SwapCached. */
    if (isnan(swap_cached))
        swap_used = swap_total - swap_free;
    else
        swap_used = swap_total - (swap_free + swap_cached);
    assert(!isnan(swap_used));

    if (swap_used < 0.0)
        return EINVAL;

    metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(swap_used * 1024.0), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE(swap_free * 1024.0), NULL, NULL);
    if (!isnan(swap_cached))
        metric_family_append(&fams[FAM_SWAP_CACHED_BYTES],
                             VALUE_GAUGE(swap_cached * 1024.0), NULL, NULL);

    return 0;
}

static int swap_read_io(void)
{
    FILE *fh = fopen(path_proc_vmstat, "r");
    if (fh == NULL) {
        PLUGIN_WARNING("Cannot open '%s': %s", path_proc_vmstat, STRERRNO);
        return -1;
    }

    uint8_t have_data = 0;
    uint64_t swap_in = 0;
    uint64_t swap_out = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fh) != NULL) {
        char *fields[8];
        int numfields = strsplit(buffer, fields, STATIC_ARRAY_SIZE(fields));

        if (numfields != 2)
            continue;

        if (strcasecmp("pswpin", fields[0]) == 0) {
            strtouint(fields[1], &swap_in);
            have_data |= 0x01;
        } else if (strcasecmp("pswpout", fields[0]) == 0) {
            strtouint(fields[1], &swap_out);
            have_data |= 0x02;
        }
    }

    fclose(fh);

    if (have_data != 0x03)
        return ENOENT;

    swap_in = swap_in * pagesize;
    swap_out = swap_out * pagesize;

    metric_family_append(&fams[FAM_SWAP_IN_BYTES], VALUE_COUNTER(swap_in), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_OUT_BYTES], VALUE_COUNTER(swap_out), NULL, NULL);

    return 0;
}

static int swap_read(void)
{
    if (report_by_device)
        swap_read_separate();
    else
        swap_read_combined();

    swap_read_io();

    plugin_dispatch_metric_family_array(fams, FAM_SWAP_MAX, 0);
    return 0;
}

/*
 * Under Solaris, two mechanisms can be used to read swap statistics, swapctl
 * and kstat. The former reads physical space used on a device, the latter
 * reports the view from the virtual memory system. It was decided that the
 * kstat-based information should be moved to the "vmem" plugin, but nobody
 * with enough Solaris experience was available at that time to do this. The
 * code below is still there for your reference but it won't be activated in
 * *this* plugin again. --octo
 */
#elif 0 && defined(HAVE_LIBKSTAT)
/* kstat-based read function */
static int swap_read(void)
{
    struct anoninfo ai;
    if (swapctl(SC_AINFO, &ai) == -1) {
        PLUGIN_ERROR("swapctl failed: %s", STRERRNO);
        return -1;
    }

    /*
     * Calculations from:
     * http://cvs.opensolaris.org/source/xref/on/usr/src/cmd/swap/swap.c
     * Also see:
     * http://www.itworld.com/Comp/2377/UIR980701perf/ (outdated?)
     * /usr/include/vm/anon.h
     *
     * In short, swap -s shows: allocated + reserved = used, available
     *
     * However, Solaris does not allow to allocated/reserved more than the
     * available swap (physical memory + disk swap), so the pedant may
     * prefer: allocated + unallocated = reserved, available
     *
     * We map the above to: used + resv = n/a, free
     *
     * Does your brain hurt yet?    - Christophe Kalt
     *
     * Oh, and in case you wonder,
     * swap_alloc = pagesize * ( ai.ani_max - ai.ani_free );
     * can suffer from a 32bit overflow.
     */
    double swap_alloc = (double)((ai.ani_max - ai.ani_free) * pagesize);
    double swap_resv = (double)((ai.ani_resv + ai.ani_free - ai.ani_max) * pagesize);
    double swap_avail = (double)((ai.ani_max - ai.ani_resv) * pagesize);

    metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(swap_alloc), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE(swap_avail), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_RESERVED_BYTES], VALUE_GAUGE(swap_resv), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SWAP_MAX, 0);
    return 0;
}
    /* #endif 0 && HAVE_LIBKSTAT */

#elif defined(HAVE_SWAPCTL) && defined(HAVE_SWAPCTL_TWO_ARGS)
/* swapctl-based read function */
static int swap_read(void)
{
    int swap_num = swapctl(SC_GETNSWP, NULL);
    if (swap_num < 0) {
        PLUGIN_ERROR("swapctl (SC_GETNSWP) failed with status %i.", swap_num);
        return -1;
    } else if (swap_num == 0)
        return 0;

    /* Allocate and initialize the swaptbl_t structure */
    swaptbl_t *s = malloc(swap_num * sizeof(swapent_t) + sizeof(struct swaptable));
    if (s == NULL) {
        PLUGIN_ERROR("malloc failed.");
        return -1;
    }

    /* Memory to store the path names. We only use these paths when the
     * separate option has been configured, but it's easier to just
     * allocate enough memory in any case. */
    char *s_paths = calloc(swap_num, PATH_MAX);
    if (s_paths == NULL) {
        PLUGIN_ERROR("calloc failed.");
        free(s);
        return -1;
    }
    for (int i = 0; i < swap_num; i++)
        s->swt_ent[i].ste_path = s_paths + (i * PATH_MAX);
    s->swt_n = swap_num;

    int status = swapctl(SC_LIST, s);
    if (status < 0) {
        PLUGIN_ERROR("swapctl (SC_LIST) failed: %s", STRERRNO);
        free(s_paths);
        free(s);
        return -1;
    } else if (swap_num < status) {
        /* more elements returned than requested */
        PLUGIN_ERROR("I allocated memory for %i structure%s, "
                     "but swapctl(2) claims to have returned %i. "
                     "I'm confused and will give up.",
                     swap_num, (swap_num == 1) ? "" : "s", status);
        free(s_paths);
        free(s);
        return -1;
    } else if (swap_num > status)
        /* less elements returned than requested */
        swap_num = status;

    double avail = 0;
    double total = 0;

    for (int i = 0; i < swap_num; i++) {
        char path[PATH_MAX];
        double this_total;
        double this_avail;

        if ((s->swt_ent[i].ste_flags & ST_INDEL) != 0)
            continue;

        this_total = (double)(s->swt_ent[i].ste_pages * pagesize);
        this_avail = (double)(s->swt_ent[i].ste_free * pagesize);

        /* Shortcut for the "combined" setting (default) */
        if (!report_by_device) {
            avail += this_avail;
            total += this_total;
            continue;
        }

        sstrncpy(path, s->swt_ent[i].ste_path, sizeof(path));

        metric_family_append(&fams[FAM_SWAP_USED_BYTES],
                             VALUE_GAUGE(this_total - this_avail), NULL,
                             &(label_pair_const_t){.name="device", .value=path}, NULL);
        metric_family_append(&fams[FAM_SWAP_FREE_BYTES],
                             VALUE_GAUGE(this_avail), NULL,
                             &(label_pair_const_t){.name="device", .value=path}, NULL);
    }

    if (total < avail) {
        PLUGIN_ERROR("Total swap space (%g) is less than free swap space (%g).", total, avail);
        free(s_paths);
        free(s);
        return -1;
    }

    /* If the "separate" option was specified (report_by_device == true) all
     * values have already been dispatched from within the loop. */
    if (!report_by_device) {
        metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(total - avail), NULL, NULL);
        metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE(avail), NULL, NULL);
    }

    free(s_paths);
    free(s);
    return 0;
}
    /* #endif HAVE_SWAPCTL && HAVE_SWAPCTL_TWO_ARGS */

#elif defined(HAVE_SWAPCTL) && defined(HAVE_SWAPCTL_THREE_ARGS)
#ifdef KERNEL_NETBSD
#include <uvm/uvm_extern.h>

static int swap_read_io(void)
{
    int uvmexp_mib[] = {CTL_VM, VM_UVMEXP2};
    struct uvmexp_sysctl uvmexp = {0};
    size_t ssize = sizeof(uvmexp);
    if (sysctl(uvmexp_mib, __arraycount(uvmexp_mib), &uvmexp, &ssize, NULL, 0) == -1) {
        char errbuf[1024];
        PLUGIN_WARNING("sysctl for uvmexp failed: %s", sstrerror(errno, errbuf, sizeof(errbuf)));
        return (-1);
    }

    uint64_t swap_in = uvmexp.pgswapin * pagesize;
    uint64_t swap_out = uvmexp.pgswapout * pagesize;

    metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_COUNTER(swap_in), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_COUNTER(swap_out), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SWAP_MAX, 0);

    return (0);
}
#endif

static int swap_read(void)
{
    int swap_num = swapctl(SWAP_NSWAP, NULL, 0);
    if (swap_num < 0) {
        PLUGIN_ERROR("swapctl (SWAP_NSWAP) failed with status %i.", swap_num);
        return -1;
    } else if (swap_num == 0)
        return 0;

    struct swapent *swap_entries = calloc(swap_num, sizeof(*swap_entries));
    if (swap_entries == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = swapctl(SWAP_STATS, swap_entries, swap_num);
    if (status != swap_num) {
        PLUGIN_ERROR("swapctl (SWAP_STATS) failed with status %i.", status);
        free(swap_entries);
        return -1;
    }

#if defined(DEV_BSIZE) && (DEV_BSIZE > 0)
#define C_SWAP_BLOCK_SIZE ((double)DEV_BSIZE)
#else
#define C_SWAP_BLOCK_SIZE 512.0
#endif

    double used = 0;
    double total = 0;
    /* TODO: Report per-device stats. The path name is available from
     * swap_entries[i].se_path */
    for (int i = 0; i < swap_num; i++) {
        char path[PATH_MAX];
        double this_used;
        double this_total;

        if ((swap_entries[i].se_flags & SWF_ENABLE) == 0)
            continue;

        this_used = ((double)swap_entries[i].se_inuse) * C_SWAP_BLOCK_SIZE;
        this_total = ((double)swap_entries[i].se_nblks) * C_SWAP_BLOCK_SIZE;

        /* Shortcut for the "combined" setting (default) */
        if (!report_by_device) {
            used += this_used;
            total += this_total;
            continue;
        }

        sstrncpy(path, swap_entries[i].se_path, sizeof(path));

        metric_family_append(&fams[FAM_SWAP_USED_BYTES],
                             VALUE_GAUGE(this_used), NULL,
                             &(label_pair_const_t){.name="device", .value=path}, NULL);
        metric_family_append(&fams[FAM_SWAP_FREE_BYTES],
                             VALUE_GAUGE(this_total - this_used), NULL,
                             &(label_pair_const_t){.name="device", .value=path}, NULL);
    }

    if (total < used) {
        PLUGIN_ERROR("Total swap space (%g) is less than used swap space (%g).", total, used);
        free(swap_entries);
        return -1;
    }

    /* If the "separate" option was specified (report_by_device == 1), all
     * values have already been dispatched from within the loop. */
    if (!report_by_device) {
        metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(used), NULL, NULL);
        metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE(total - used), NULL, NULL);
    }

    free(swap_entries);
#ifdef KERNEL_NETBSD
    swap_read_io();
#endif
    return 0;
}
    /* #endif HAVE_SWAPCTL && HAVE_SWAPCTL_THREE_ARGS */

#elif defined(VM_SWAPUSAGE)
static int swap_read(void)
{
    size_t mib_len = 2;
    int mib[3] = {CTL_VM, VM_SWAPUSAGE};
    struct xsw_usage sw_usage;
    size_t sw_usage_len = sizeof(struct xsw_usage);

    if (sysctl(mib, mib_len, &sw_usage, &sw_usage_len, NULL, 0) != 0)
        return -1;

    metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(sw_usage.xsu_used), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE(sw_usage.xsu_avail), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SWAP_MAX, 0);
    return 0;
}
    /* #endif VM_SWAPUSAGE */

#elif defined(HAVE_LIBKVM_GETSWAPINFO)
static int swap_read(void)
{
    if (kvm_obj == NULL)
        return -1;

    /* only one structure => only get the grand total, no details */
    struct kvm_swap data_s;
    int status = kvm_getswapinfo(kvm_obj, &data_s, 1, 0);
    if (status == -1)
        return -1;

    double total = (double)data_s.ksw_total * kvm_pagesize;
    double used = (double)data_s.ksw_used * kvm_pagesize;

    metric_family_append(&fams[FAM_SWAP_USED_BYTES], VALUE_GAUGE(used), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_FREE_BYTES], VALUE_GAUGE(total - used), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SWAP_MAX, 0);

    return 0;
}
    /* #endif HAVE_LIBKVM_GETSWAPINFO */
#elif defined(HAVE_PERFSTAT)
static int swap_read(void)
{
    perfstat_memory_total_t pmemory = {0};
    int status = perfstat_memory_total(NULL, &pmemory, sizeof(perfstat_memory_total_t), 1);
    if (status < 0) {
        PLUGIN_WARNING("perfstat_memory_total failed: %s", STRERRNO);
        return -1;
    }

    metric_family_append(&fams[FAM_SWAP_USED_BYTES],
                         VALUE_GAUGE((pmemory.pgsp_total-pmemory.pgsp_free) * pagesize), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_FREE_BYTES],
                         VALUE_GAUGE((pmemory.pgsp_free * pagesize)), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_RESERVED_BYTES],
                         VALUE_GAUGE((pmemory.pgsp_rsvd * pagesize)), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_IN_BYTES],
                         VALUE_COUNTER((pmemory.pgspins * pagesize)), NULL, NULL);
    metric_family_append(&fams[FAM_SWAP_OUT_BYTES],
                         VALUE_COUNTER((pmemory.pgspouts * pagesize)), NULL, NULL);

    plugin_dispatch_metric_family_array(fams, FAM_SWAP_MAX, 0);

    return 0;
}
#endif /* HAVE_PERFSTAT */

static int swap_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;
        if (strcasecmp("report-by-device", child->key) == 0) {
#ifdef SWAP_HAVE_REPORT_BY_DEVICE
            status = cf_util_get_boolean(child, &report_by_device);
#else
            PLUGIN_WARNING("The \"report-by-device\" option is not supported on this platform. "
                            "The option is going to be ignored.");
#endif
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

static int swap_init(void)
{
#ifdef KERNEL_LINUX
    path_proc_swaps = plugin_procpath("swaps");
    if (path_proc_swaps == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_meminfo = plugin_procpath("meminfo");
    if (path_proc_meminfo == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }

    path_proc_vmstat = plugin_procpath("vmstat");
    if (path_proc_vmstat == NULL) {
        PLUGIN_ERROR("Cannot get proc path.");
        return -1;
    }


    pagesize = (int64_t)sysconf(_SC_PAGESIZE);
/* #endif KERNEL_LINUX */

#elif defined(HAVE_SWAPCTL) && (defined(HAVE_SWAPCTL_TWO_ARGS) || defined(HAVE_SWAPCTL_THREE_ARGS))
    /* getpagesize(3C) tells me this does not fail.. */
    pagesize = (int64_t)getpagesize();
/* #endif HAVE_SWAPCTL */

#elif defined(VM_SWAPUSAGE)
    /* No init stuff */
/* #endif defined(VM_SWAPUSAGE) */

#elif defined(HAVE_LIBKVM_GETSWAPINFO)
    char errbuf[_POSIX2_LINE_MAX];

    if (kvm_obj != NULL) {
        kvm_close(kvm_obj);
        kvm_obj = NULL;
    }

    kvm_pagesize = getpagesize();

    kvm_obj = kvm_openfiles(NULL, "/dev/null", NULL, O_RDONLY, errbuf);
    if (kvm_obj == NULL) {
        PLUGIN_ERROR("kvm_openfiles failed, %s", errbuf);
        return -1;
    }
/* #endif HAVE_LIBKVM_GETSWAPINFO */

#elif defined(HAVE_PERFSTAT)
    pagesize = getpagesize();
#endif /* HAVE_PERFSTAT */

    return 0;
}

#ifdef KERNEL_LINUX
static int swap_shutdown(void)
{
    free(path_proc_swaps);
    free(path_proc_meminfo);
    free(path_proc_vmstat);
    return 0;
}
#endif

void module_register(void)
{
    plugin_register_config("swap", swap_config);
    plugin_register_init("swap", swap_init);
    plugin_register_read("swap", swap_read);
#ifdef KERNEL_LINUX
    plugin_register_shutdown("swap", swap_shutdown);
#endif
}
