// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009  Anthony Dewhurst
// SPDX-FileCopyrightText: Copyright (C) 2012  Aurelien Rougemont
// SPDX-FileCopyrightText: Copyright (C) 2013  Xin Li
// SPDX-FileCopyrightText: Copyright (C) 2014  Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2014  Wilfried Goesgens
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Anthony Dewhurst <dewhurst at gmail>
// SPDX-FileContributor: Aurelien Rougemont <beorn at gandi.net>
// SPDX-FileContributor: Xin Li <delphij at FreeBSD.org>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Wilfried Goesgens <dothebart at citadel.org>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/kstat.h"

#if 0
static value_to_rate_state_t arc_hits_state;
static value_to_rate_state_t arc_misses_state;
static value_to_rate_state_t l2_hits_state;
static value_to_rate_state_t l2_misses_state;
#endif

static kstat_ctl_t *kc;

static long long get_zfs_value(kstat_t *ksp, char *name)
{
    return get_kstat_value(ksp, name);
}

int zfs_read(void)
{
    gauge_t arc_hits, arc_misses, l2_hits, l2_misses;
    kstat_t *ksp = NULL;

    if (kc == NULL)
        return -1;

    if (kstat_chain_update(kc) < 0) {
        PLUGIN_ERROR("kstat_chain_update failed.");
        return -1;
    }

    get_kstat(kc, &ksp, "zfs", 0, "arcstats");
    if (ksp == NULL) {
        PLUGIN_ERROR("Cannot find zfs:0:arcstats kstat.");
        return -1;
    }

    /* Sizes */
    za_read_gauge(ksp, "anon_size",      "cache_size", "anon_size");
    za_read_gauge(ksp, "c",              "cache_size", "c");
    za_read_gauge(ksp, "c_max",          "cache_size", "c_max");
    za_read_gauge(ksp, "c_min",          "cache_size", "c_min");
    za_read_gauge(ksp, "hdr_size",       "cache_size", "hdr_size");
    za_read_gauge(ksp, "metadata_size",  "cache_size", "metadata_size");
    za_read_gauge(ksp, "mfu_ghost_size", "cache_size", "mfu_ghost_size");
    za_read_gauge(ksp, "mfu_size",       "cache_size", "mfu_size");
    za_read_gauge(ksp, "mru_ghost_size", "cache_size", "mru_ghost_size");
    za_read_gauge(ksp, "mru_size",       "cache_size", "mru_size");
    za_read_gauge(ksp, "p",              "cache_size", "p");
    za_read_gauge(ksp, "size",           "cache_size", "arc");

    /* The "other_size" value was replaced by more specific values in ZFS on Linux
     * version 0.7.0 (commit 25458cb)
     */
    if (za_read_gauge(ksp, "dbuf_size", "cache_size", "dbuf_size") != 0 ||
            za_read_gauge(ksp, "dnode_size", "cache_size", "dnode_size") != 0 ||
            za_read_gauge(ksp, "bonus_size", "cache_size", "bonus_size") != 0)
        za_read_gauge(ksp, "other_size", "cache_size", "other_size");

    /* The "l2_size" value has disappeared from Solaris some time in
     * early 2013, and has only reappeared recently in Solaris 11.2.
     * Stop trying if we ever fail to read it, so we don't spam the log.
     */
    static int l2_size_avail = 1;
    if (l2_size_avail && za_read_gauge(ksp, "l2_size", "cache_size", "L2") != 0)
        l2_size_avail = 0;

    /* Operations */
    za_read_derive(ksp, "deleted", "cache_operation", "deleted");

    /* Issue indicators */
    za_read_derive(ksp, "mutex_miss",            "mutex_operations", "miss");
    za_read_derive(ksp, "hash_collisions",       "hash_collisions", "");
    za_read_derive(ksp, "memory_throttle_count", "memory_throttle_count", "");

    /* Evictions */
    za_read_derive(ksp, "evict_l2_cached",     "cache_eviction", "cached");
    za_read_derive(ksp, "evict_l2_eligible",   "cache_eviction", "eligible");
    za_read_derive(ksp, "evict_l2_ineligible", "cache_eviction", "ineligible");

    /* Hits / misses */
    za_read_derive(ksp, "demand_data_hits",         "cache_result", "demand_data-hit");
    za_read_derive(ksp, "demand_metadata_hits",     "cache_result", "demand_metadata-hit");
    za_read_derive(ksp, "prefetch_data_hits",       "cache_result", "prefetch_data-hit");
    za_read_derive(ksp, "prefetch_metadata_hits",   "cache_result", "prefetch_metadata-hit");
    za_read_derive(ksp, "demand_data_misses",       "cache_result", "demand_data-miss");
    za_read_derive(ksp, "demand_metadata_misses",   "cache_result", "demand_metadata-miss");
    za_read_derive(ksp, "prefetch_data_misses",     "cache_result", "prefetch_data-miss");
    za_read_derive(ksp, "prefetch_metadata_misses", "cache_result", "prefetch_metadata-miss");
    za_read_derive(ksp, "mfu_hits",                 "cache_result", "mfu-hit");
    za_read_derive(ksp, "mfu_ghost_hits",           "cache_result", "mfu_ghost-hit");
    za_read_derive(ksp, "mru_hits",                 "cache_result", "mru-hit");
    za_read_derive(ksp, "mru_ghost_hits",           "cache_result", "mru_ghost-hit");

    cdtime_t now = cdtime();
#if 0
    /* Ratios */
    if ((value_to_rate(&arc_hits, (value_t){.derive = get_zfs_value(ksp, "hits")}, DS_TYPE_DERIVE, now, &arc_hits_state) == 0) &&
        (value_to_rate(&arc_misses, (value_t){.derive = get_zfs_value(ksp, "misses")}, DS_TYPE_DERIVE, now, &arc_misses_state) == 0)) {
        za_submit_ratio("arc", arc_hits, arc_misses);
    }

    if ((value_to_rate(&l2_hits, (value_t){.derive = get_zfs_value(ksp, "l2_hits")}, DS_TYPE_DERIVE, now, &l2_hits_state) == 0) &&
        (value_to_rate(&l2_misses, (value_t){.derive = get_zfs_value(ksp, "l2_misses")}, DS_TYPE_DERIVE, now, &l2_misses_state) == 0)) {
        za_submit_ratio("L2", l2_hits, l2_misses);
    }
#endif
    /* I/O */
    value_t l2_io[] = {
            {.derive = (derive_t)get_zfs_value(ksp, "l2_read_bytes")},
            {.derive = (derive_t)get_zfs_value(ksp, "l2_write_bytes")},
    };
    za_submit("io_octets", "L2", l2_io, STATIC_ARRAY_SIZE(l2_io));

    return 0;
}

int zfs_init(void)
{
    if (kc == NULL)
        kc = kstat_open();

    if (kc == NULL) {
        PLUGIN_ERROR("kstat_open failed.");
        return -1;
    }

    return 0;
}
