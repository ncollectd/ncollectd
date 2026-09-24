// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libconfig/config.h"

#include <vapi/vsc.h>
#include <vapi/vsm.h>

extern void module_register(void);

static struct {
    char *name;
    int semantics;
    int format;
    uint64_t value;
} vsh_metrics[] = {
    { .name = "MGT.uptime",                          .semantics = 'c', .format = 'd', .value = 7534733  },
    { .name = "MGT.child_start",                     .semantics = 'c', .format = 'i', .value = 16550171 },
    { .name = "MGT.child_exit",                      .semantics = 'c', .format = 'i', .value = 11876743 },
    { .name = "MGT.child_stop",                      .semantics = 'c', .format = 'i', .value = 3538028  },
    { .name = "MGT.child_died",                      .semantics = 'c', .format = 'i', .value = 7950621  },
    { .name = "MGT.child_dump",                      .semantics = 'c', .format = 'i', .value = 14515332 },
    { .name = "MGT.child_panic",                     .semantics = 'c', .format = 'i', .value = 1575707  },
    { .name = "MAIN.summs",                          .semantics = 'c', .format = 'i', .value = 1670327  },
    { .name = "MAIN.uptime",                         .semantics = 'c', .format = 'd', .value = 6423927  },
    { .name = "MAIN.sess_conn",                      .semantics = 'c', .format = 'i', .value = 5062744  },
    { .name = "MAIN.sess_fail",                      .semantics = 'c', .format = 'i', .value = 11024642 },
    { .name = "MAIN.sess_fail_econnaborted",         .semantics = 'c', .format = 'i', .value = 13574366 },
    { .name = "MAIN.sess_fail_eintr",                .semantics = 'c', .format = 'i', .value = 2209595  },
    { .name = "MAIN.sess_fail_emfile",               .semantics = 'c', .format = 'i', .value = 864165   },
    { .name = "MAIN.sess_fail_ebadf",                .semantics = 'c', .format = 'i', .value = 896277   },
    { .name = "MAIN.sess_fail_enomem",               .semantics = 'c', .format = 'i', .value = 7679193  },
    { .name = "MAIN.sess_fail_other",                .semantics = 'c', .format = 'i', .value = 13100797 },
    { .name = "MAIN.client_req_400",                 .semantics = 'c', .format = 'i', .value = 11611115 },
    { .name = "MAIN.client_req_417",                 .semantics = 'c', .format = 'i', .value = 7424928  },
    { .name = "MAIN.client_req",                     .semantics = 'c', .format = 'i', .value = 1998355  },
    { .name = "MAIN.esi_req",                        .semantics = 'c', .format = 'i', .value = 9892462  },
    { .name = "MAIN.cache_hit",                      .semantics = 'c', .format = 'i', .value = 9707881  },
    { .name = "MAIN.cache_hit_grace",                .semantics = 'c', .format = 'i', .value = 8890233  },
    { .name = "MAIN.cache_hitpass",                  .semantics = 'c', .format = 'i', .value = 9983202  },
    { .name = "MAIN.cache_hitmiss",                  .semantics = 'c', .format = 'i', .value = 6071957  },
    { .name = "MAIN.cache_miss",                     .semantics = 'c', .format = 'i', .value = 5105062  },
    { .name = "MAIN.beresp_uncacheable",             .semantics = 'c', .format = 'i', .value = 14910303 },
    { .name = "MAIN.beresp_shortlived",              .semantics = 'c', .format = 'i', .value = 7995763  },
    { .name = "MAIN.backend_conn",                   .semantics = 'c', .format = 'i', .value = 2849111  },
    { .name = "MAIN.backend_unhealthy",              .semantics = 'c', .format = 'i', .value = 10229557 },
    { .name = "MAIN.backend_busy",                   .semantics = 'c', .format = 'i', .value = 8820572  },
    { .name = "MAIN.backend_fail",                   .semantics = 'c', .format = 'i', .value = 10383844 },
    { .name = "MAIN.backend_reuse",                  .semantics = 'c', .format = 'i', .value = 10002512 },
    { .name = "MAIN.backend_recycle",                .semantics = 'c', .format = 'i', .value = 3920100  },
    { .name = "MAIN.backend_retry",                  .semantics = 'c', .format = 'i', .value = 13921873 },
    { .name = "MAIN.backend_wait",                   .semantics = 'c', .format = 'i', .value = 1175918  },
    { .name = "MAIN.backend_wait_fail",              .semantics = 'c', .format = 'i', .value = 1658216  },
    { .name = "MAIN.fetch_head",                     .semantics = 'c', .format = 'i', .value = 15497581 },
    { .name = "MAIN.fetch_length",                   .semantics = 'c', .format = 'i', .value = 2846246  },
    { .name = "MAIN.fetch_chunked",                  .semantics = 'c', .format = 'i', .value = 8082144  },
    { .name = "MAIN.fetch_eof",                      .semantics = 'c', .format = 'i', .value = 3783109  },
    { .name = "MAIN.fetch_bad",                      .semantics = 'c', .format = 'i', .value = 13870888 },
    { .name = "MAIN.fetch_none",                     .semantics = 'c', .format = 'i', .value = 4879295  },
    { .name = "MAIN.fetch_1xx",                      .semantics = 'c', .format = 'i', .value = 5992705  },
    { .name = "MAIN.fetch_204",                      .semantics = 'c', .format = 'i', .value = 14735054 },
    { .name = "MAIN.fetch_304",                      .semantics = 'c', .format = 'i', .value = 5775572  },
    { .name = "MAIN.fetch_failed",                   .semantics = 'c', .format = 'i', .value = 13671898 },
    { .name = "MAIN.bgfetch_no_thread",              .semantics = 'c', .format = 'i', .value = 11058635 },
    { .name = "MAIN.pools",                          .semantics = 'g', .format = 'i', .value = 609472   },
    { .name = "MAIN.threads",                        .semantics = 'g', .format = 'i', .value = 4319611  },
    { .name = "MAIN.threads_limited",                .semantics = 'c', .format = 'i', .value = 13056991 },
    { .name = "MAIN.threads_created",                .semantics = 'c', .format = 'i', .value = 10501935 },
    { .name = "MAIN.threads_destroyed",              .semantics = 'c', .format = 'i', .value = 14027493 },
    { .name = "MAIN.threads_failed",                 .semantics = 'c', .format = 'i', .value = 5170009  },
    { .name = "MAIN.thread_queue_len",               .semantics = 'g', .format = 'i', .value = 3707921  },
    { .name = "MAIN.busy_sleep",                     .semantics = 'c', .format = 'i', .value = 3322234  },
    { .name = "MAIN.busy_wakeup",                    .semantics = 'c', .format = 'i', .value = 10275071 },
    { .name = "MAIN.busy_killed",                    .semantics = 'c', .format = 'i', .value = 1841009  },
    { .name = "MAIN.sess_queued",                    .semantics = 'c', .format = 'i', .value = 11317997 },
    { .name = "MAIN.sess_dropped",                   .semantics = 'c', .format = 'i', .value = 13124183 },
    { .name = "MAIN.req_dropped",                    .semantics = 'c', .format = 'i', .value = 12070566 },
    { .name = "MAIN.req_reset",                      .semantics = 'c', .format = 'i', .value = 3361354  },
    { .name = "MAIN.n_object",                       .semantics = 'g', .format = 'i', .value = 6730811  },
    { .name = "MAIN.n_vampireobject",                .semantics = 'g', .format = 'i', .value = 5295862  },
    { .name = "MAIN.n_objectcore",                   .semantics = 'g', .format = 'i', .value = 7281455  },
    { .name = "MAIN.n_objecthead",                   .semantics = 'g', .format = 'i', .value = 3875468  },
    { .name = "MAIN.n_backend",                      .semantics = 'g', .format = 'i', .value = 6471781  },
    { .name = "MAIN.n_expired",                      .semantics = 'c', .format = 'i', .value = 8939672  },
    { .name = "MAIN.n_superseded",                   .semantics = 'c', .format = 'i', .value = 2595833  },
    { .name = "MAIN.n_lru_nuked",                    .semantics = 'c', .format = 'i', .value = 9318027  },
    { .name = "MAIN.n_lru_moved",                    .semantics = 'c', .format = 'i', .value = 244600   },
    { .name = "MAIN.n_lru_limited",                  .semantics = 'c', .format = 'i', .value = 6378943  },
    { .name = "MAIN.losthdr",                        .semantics = 'c', .format = 'i', .value = 6411700  },
    { .name = "MAIN.s_sess",                         .semantics = 'c', .format = 'i', .value = 5123896  },
    { .name = "MAIN.n_pipe",                         .semantics = 'g', .format = 'i', .value = 12371648 },
    { .name = "MAIN.pipe_limited",                   .semantics = 'c', .format = 'i', .value = 4369539  },
    { .name = "MAIN.s_pipe",                         .semantics = 'c', .format = 'i', .value = 10899468 },
    { .name = "MAIN.s_pass",                         .semantics = 'c', .format = 'i', .value = 9266331  },
    { .name = "MAIN.s_fetch",                        .semantics = 'c', .format = 'i', .value = 15428175 },
    { .name = "MAIN.s_bgfetch",                      .semantics = 'c', .format = 'i', .value = 11508941 },
    { .name = "MAIN.s_synth",                        .semantics = 'c', .format = 'i', .value = 13585942 },
    { .name = "MAIN.s_req_hdrbytes",                 .semantics = 'c', .format = 'B', .value = 11707950 },
    { .name = "MAIN.s_req_bodybytes",                .semantics = 'c', .format = 'B', .value = 5233660  },
    { .name = "MAIN.s_resp_hdrbytes",                .semantics = 'c', .format = 'B', .value = 10836219 },
    { .name = "MAIN.s_resp_bodybytes",               .semantics = 'c', .format = 'B', .value = 100743   },
    { .name = "MAIN.s_pipe_hdrbytes",                .semantics = 'c', .format = 'B', .value = 8941582  },
    { .name = "MAIN.s_pipe_in",                      .semantics = 'c', .format = 'B', .value = 14158454 },
    { .name = "MAIN.s_pipe_out",                     .semantics = 'c', .format = 'B', .value = 10375815 },
    { .name = "MAIN.transit_stored",                 .semantics = 'c', .format = 'B', .value = 10782591 },
    { .name = "MAIN.transit_buffered",               .semantics = 'c', .format = 'B', .value = 8699236  },
    { .name = "MAIN.sess_closed",                    .semantics = 'c', .format = 'i', .value = 6722782  },
    { .name = "MAIN.sess_closed_err",                .semantics = 'c', .format = 'i', .value = 6075941  },
    { .name = "MAIN.sess_readahead",                 .semantics = 'c', .format = 'i', .value = 12060590 },
    { .name = "MAIN.sess_herd",                      .semantics = 'c', .format = 'i', .value = 13453594 },
    { .name = "MAIN.sc_rem_close",                   .semantics = 'c', .format = 'i', .value = 11371804 },
    { .name = "MAIN.sc_req_close",                   .semantics = 'c', .format = 'i', .value = 2564830  },
    { .name = "MAIN.sc_req_http10",                  .semantics = 'c', .format = 'i', .value = 551847   },
    { .name = "MAIN.sc_rx_bad",                      .semantics = 'c', .format = 'i', .value = 1066369  },
    { .name = "MAIN.sc_rx_body",                     .semantics = 'c', .format = 'i', .value = 11504502 },
    { .name = "MAIN.sc_rx_junk",                     .semantics = 'c', .format = 'i', .value = 3147681  },
    { .name = "MAIN.sc_rx_overflow",                 .semantics = 'c', .format = 'i', .value = 10384397 },
    { .name = "MAIN.sc_rx_timeout",                  .semantics = 'c', .format = 'i', .value = 11749103 },
    { .name = "MAIN.sc_rx_close_idle",               .semantics = 'c', .format = 'i', .value = 9526624  },
    { .name = "MAIN.sc_tx_pipe",                     .semantics = 'c', .format = 'i', .value = 18882    },
    { .name = "MAIN.sc_tx_error",                    .semantics = 'c', .format = 'i', .value = 95783    },
    { .name = "MAIN.sc_tx_eof",                      .semantics = 'c', .format = 'i', .value = 5121057  },
    { .name = "MAIN.sc_resp_close",                  .semantics = 'c', .format = 'i', .value = 4388421  },
    { .name = "MAIN.sc_overload",                    .semantics = 'c', .format = 'i', .value = 10995252 },
    { .name = "MAIN.sc_pipe_overflow",               .semantics = 'c', .format = 'i', .value = 14387389 },
    { .name = "MAIN.sc_range_short",                 .semantics = 'c', .format = 'i', .value = 3039380  },
    { .name = "MAIN.sc_req_http20",                  .semantics = 'c', .format = 'i', .value = 5726977  },
    { .name = "MAIN.sc_vcl_failure",                 .semantics = 'c', .format = 'i', .value = 11196115 },
    { .name = "MAIN.sc_rapid_reset",                 .semantics = 'c', .format = 'i', .value = 14747331 },
    { .name = "MAIN.sc_bankrupt",                    .semantics = 'c', .format = 'i', .value = 10960638 },
    { .name = "MAIN.client_resp_500",                .semantics = 'c', .format = 'i', .value = 5255119  },
    { .name = "MAIN.ws_backend_overflow",            .semantics = 'c', .format = 'i', .value = 14848074 },
    { .name = "MAIN.ws_client_overflow",             .semantics = 'c', .format = 'i', .value = 3125004  },
    { .name = "MAIN.ws_thread_overflow",             .semantics = 'c', .format = 'i', .value = 2636358  },
    { .name = "MAIN.ws_session_overflow",            .semantics = 'c', .format = 'i', .value = 8446673  },
    { .name = "MAIN.shm_records",                    .semantics = 'c', .format = 'i', .value = 13907595 },
    { .name = "MAIN.shm_writes",                     .semantics = 'c', .format = 'i', .value = 11335594 },
    { .name = "MAIN.shm_flushes",                    .semantics = 'c', .format = 'i', .value = 15169455 },
    { .name = "MAIN.shm_cont",                       .semantics = 'c', .format = 'i', .value = 3206321  },
    { .name = "MAIN.shm_cycles",                     .semantics = 'c', .format = 'i', .value = 6618968  },
    { .name = "MAIN.shm_bytes",                      .semantics = 'c', .format = 'B', .value = 11845834 },
    { .name = "MAIN.backend_req",                    .semantics = 'c', .format = 'i', .value = 14578125 },
    { .name = "MAIN.n_vcl",                          .semantics = 'g', .format = 'i', .value = 9183799  },
    { .name = "MAIN.n_vcl_avail",                    .semantics = 'g', .format = 'i', .value = 12397681 },
    { .name = "MAIN.n_vcl_discard",                  .semantics = 'g', .format = 'i', .value = 15644495 },
    { .name = "MAIN.vcl_fail",                       .semantics = 'c', .format = 'i', .value = 3911085  },
    { .name = "MAIN.bans",                           .semantics = 'g', .format = 'i', .value = 15545362 },
    { .name = "MAIN.bans_completed",                 .semantics = 'g', .format = 'i', .value = 9251677  },
    { .name = "MAIN.bans_obj",                       .semantics = 'g', .format = 'i', .value = 15660189 },
    { .name = "MAIN.bans_req",                       .semantics = 'g', .format = 'i', .value = 8294771  },
    { .name = "MAIN.bans_added",                     .semantics = 'c', .format = 'i', .value = 9270559  },
    { .name = "MAIN.bans_deleted",                   .semantics = 'c', .format = 'i', .value = 15755972 },
    { .name = "MAIN.bans_tested",                    .semantics = 'c', .format = 'i', .value = 13415828 },
    { .name = "MAIN.bans_obj_killed",                .semantics = 'c', .format = 'i', .value = 13658981 },
    { .name = "MAIN.bans_lurker_tested",             .semantics = 'c', .format = 'i', .value = 9974009  },
    { .name = "MAIN.bans_tests_tested",              .semantics = 'c', .format = 'i', .value = 11026001 },
    { .name = "MAIN.bans_lurker_tests_tested",       .semantics = 'c', .format = 'i', .value = 16698362 },
    { .name = "MAIN.bans_lurker_obj_killed",         .semantics = 'c', .format = 'i', .value = 15700986 },
    { .name = "MAIN.bans_lurker_obj_killed_cutoff",  .semantics = 'c', .format = 'i', .value = 5444901  },
    { .name = "MAIN.bans_dups",                      .semantics = 'c', .format = 'i', .value = 14668477 },
    { .name = "MAIN.bans_lurker_contention",         .semantics = 'c', .format = 'i', .value = 9884408  },
    { .name = "MAIN.bans_persisted_bytes",           .semantics = 'g', .format = 'B', .value = 10700021 },
    { .name = "MAIN.bans_persisted_fragmentation",   .semantics = 'g', .format = 'B', .value = 12739335 },
    { .name = "MAIN.n_purges",                       .semantics = 'c', .format = 'i', .value = 13009413 },
    { .name = "MAIN.n_obj_purged",                   .semantics = 'c', .format = 'i', .value = 13336379 },
    { .name = "MAIN.exp_mailed",                     .semantics = 'c', .format = 'i', .value = 4408793  },
    { .name = "MAIN.exp_received",                   .semantics = 'c', .format = 'i', .value = 10139793 },
    { .name = "MAIN.hcb_nolock",                     .semantics = 'c', .format = 'i', .value = 7894757  },
    { .name = "MAIN.hcb_lock",                       .semantics = 'c', .format = 'i', .value = 2801033  },
    { .name = "MAIN.hcb_insert",                     .semantics = 'c', .format = 'i', .value = 13346114 },
    { .name = "MAIN.esi_errors",                     .semantics = 'c', .format = 'i', .value = 14513726 },
    { .name = "MAIN.esi_warnings",                   .semantics = 'c', .format = 'i', .value = 14646867 },
    { .name = "MAIN.vmods",                          .semantics = 'g', .format = 'i', .value = 11147024 },
    { .name = "MAIN.n_gzip",                         .semantics = 'c', .format = 'i', .value = 6920309  },
    { .name = "MAIN.n_gunzip",                       .semantics = 'c', .format = 'i', .value = 10267332 },
    { .name = "MAIN.n_test_gunzip",                  .semantics = 'c', .format = 'i', .value = 10014303 },
    { .name = "MAIN.http1_iovs_flush",               .semantics = 'c', .format = 'i', .value = 10831395 },
    { .name = "MAIN.http1_absolute_form",            .semantics = 'c', .format = 'i', .value = 9035479  },
    { .name = "LCK.ban.creat",                       .semantics = 'c', .format = 'i', .value = 2488764  },
    { .name = "LCK.ban.destroy",                     .semantics = 'c', .format = 'i', .value = 9714368  },
    { .name = "LCK.ban.locks",                       .semantics = 'c', .format = 'i', .value = 553034   },
    { .name = "LCK.ban.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 11759324 },
    { .name = "LCK.ban.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 8693125  },
    { .name = "LCK.busyobj.creat",                   .semantics = 'c', .format = 'i', .value = 13968862 },
    { .name = "LCK.busyobj.destroy",                 .semantics = 'c', .format = 'i', .value = 8641089  },
    { .name = "LCK.busyobj.locks",                   .semantics = 'c', .format = 'i', .value = 1889918  },
    { .name = "LCK.busyobj.dbg_busy",                .semantics = 'c', .format = 'i', .value = 8217648  },
    { .name = "LCK.busyobj.dbg_try_fail",            .semantics = 'c', .format = 'i', .value = 8562235  },
    { .name = "LCK.cli.creat",                       .semantics = 'c', .format = 'i', .value = 813689   },
    { .name = "LCK.cli.destroy",                     .semantics = 'c', .format = 'i', .value = 13662549 },
    { .name = "LCK.cli.locks",                       .semantics = 'c', .format = 'i', .value = 6453497  },
    { .name = "LCK.cli.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 10698098 },
    { .name = "LCK.cli.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 7585354  },
    { .name = "LCK.director.creat",                  .semantics = 'c', .format = 'i', .value = 2415617  },
    { .name = "LCK.director.destroy",                .semantics = 'c', .format = 'i', .value = 6930296  },
    { .name = "LCK.director.locks",                  .semantics = 'c', .format = 'i', .value = 4144518  },
    { .name = "LCK.director.dbg_busy",               .semantics = 'c', .format = 'i', .value = 6824410  },
    { .name = "LCK.director.dbg_try_fail",           .semantics = 'c', .format = 'i', .value = 292873   },
    { .name = "LCK.exp.creat",                       .semantics = 'c', .format = 'i', .value = 12039275 },
    { .name = "LCK.exp.destroy",                     .semantics = 'c', .format = 'i', .value = 9625444  },
    { .name = "LCK.exp.locks",                       .semantics = 'c', .format = 'i', .value = 13638987 },
    { .name = "LCK.exp.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 9775785  },
    { .name = "LCK.exp.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 7495095  },
    { .name = "LCK.hcb.creat",                       .semantics = 'c', .format = 'i', .value = 8008795  },
    { .name = "LCK.hcb.destroy",                     .semantics = 'c', .format = 'i', .value = 16696095 },
    { .name = "LCK.hcb.locks",                       .semantics = 'c', .format = 'i', .value = 985212   },
    { .name = "LCK.hcb.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 1245883  },
    { .name = "LCK.hcb.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 10750275 },
    { .name = "LCK.lru.creat",                       .semantics = 'c', .format = 'i', .value = 10020691 },
    { .name = "LCK.lru.destroy",                     .semantics = 'c', .format = 'i', .value = 3734648  },
    { .name = "LCK.lru.locks",                       .semantics = 'c', .format = 'i', .value = 3687427  },
    { .name = "LCK.lru.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 10573725 },
    { .name = "LCK.lru.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 15493972 },
    { .name = "LCK.mempool.creat",                   .semantics = 'c', .format = 'i', .value = 12380553 },
    { .name = "LCK.mempool.destroy",                 .semantics = 'c', .format = 'i', .value = 7765372  },
    { .name = "LCK.mempool.locks",                   .semantics = 'c', .format = 'i', .value = 7357846  },
    { .name = "LCK.mempool.dbg_busy",                .semantics = 'c', .format = 'i', .value = 14270472 },
    { .name = "LCK.mempool.dbg_try_fail",            .semantics = 'c', .format = 'i', .value = 15983020 },
    { .name = "LCK.objhdr.creat",                    .semantics = 'c', .format = 'i', .value = 15920081 },
    { .name = "LCK.objhdr.destroy",                  .semantics = 'c', .format = 'i', .value = 15084162 },
    { .name = "LCK.objhdr.locks",                    .semantics = 'c', .format = 'i', .value = 12868354 },
    { .name = "LCK.objhdr.dbg_busy",                 .semantics = 'c', .format = 'i', .value = 5596363  },
    { .name = "LCK.objhdr.dbg_try_fail",             .semantics = 'c', .format = 'i', .value = 9005044  },
    { .name = "LCK.perpool.creat",                   .semantics = 'c', .format = 'i', .value = 3676492  },
    { .name = "LCK.perpool.destroy",                 .semantics = 'c', .format = 'i', .value = 8011980  },
    { .name = "LCK.perpool.locks",                   .semantics = 'c', .format = 'i', .value = 15935340 },
    { .name = "LCK.perpool.dbg_busy",                .semantics = 'c', .format = 'i', .value = 7821011  },
    { .name = "LCK.perpool.dbg_try_fail",            .semantics = 'c', .format = 'i', .value = 14836390 },
    { .name = "LCK.pipestat.creat",                  .semantics = 'c', .format = 'i', .value = 16228213 },
    { .name = "LCK.pipestat.destroy",                .semantics = 'c', .format = 'i', .value = 3083070  },
    { .name = "LCK.pipestat.locks",                  .semantics = 'c', .format = 'i', .value = 7684619  },
    { .name = "LCK.pipestat.dbg_busy",               .semantics = 'c', .format = 'i', .value = 13089985 },
    { .name = "LCK.pipestat.dbg_try_fail",           .semantics = 'c', .format = 'i', .value = 12858856 },
    { .name = "LCK.probe.creat",                     .semantics = 'c', .format = 'i', .value = 15179714 },
    { .name = "LCK.probe.destroy",                   .semantics = 'c', .format = 'i', .value = 4321565  },
    { .name = "LCK.probe.locks",                     .semantics = 'c', .format = 'i', .value = 12777736 },
    { .name = "LCK.probe.dbg_busy",                  .semantics = 'c', .format = 'i', .value = 16164927 },
    { .name = "LCK.probe.dbg_try_fail",              .semantics = 'c', .format = 'i', .value = 5567448  },
    { .name = "LCK.sess.creat",                      .semantics = 'c', .format = 'i', .value = 6750795  },
    { .name = "LCK.sess.destroy",                    .semantics = 'c', .format = 'i', .value = 9408402  },
    { .name = "LCK.sess.locks",                      .semantics = 'c', .format = 'i', .value = 9302096  },
    { .name = "LCK.sess.dbg_busy",                   .semantics = 'c', .format = 'i', .value = 10438223 },
    { .name = "LCK.sess.dbg_try_fail",               .semantics = 'c', .format = 'i', .value = 3204912  },
    { .name = "LCK.conn_pool.creat",                 .semantics = 'c', .format = 'i', .value = 8018852  },
    { .name = "LCK.conn_pool.destroy",               .semantics = 'c', .format = 'i', .value = 6041561  },
    { .name = "LCK.conn_pool.locks",                 .semantics = 'c', .format = 'i', .value = 10970284 },
    { .name = "LCK.conn_pool.dbg_busy",              .semantics = 'c', .format = 'i', .value = 15376698 },
    { .name = "LCK.conn_pool.dbg_try_fail",          .semantics = 'c', .format = 'i', .value = 3534817  },
    { .name = "LCK.dead_pool.creat",                 .semantics = 'c', .format = 'i', .value = 10176088 },
    { .name = "LCK.dead_pool.destroy",               .semantics = 'c', .format = 'i', .value = 14519564 },
    { .name = "LCK.dead_pool.locks",                 .semantics = 'c', .format = 'i', .value = 1841763  },
    { .name = "LCK.dead_pool.dbg_busy",              .semantics = 'c', .format = 'i', .value = 6267227  },
    { .name = "LCK.dead_pool.dbg_try_fail",          .semantics = 'c', .format = 'i', .value = 3338711  },
    { .name = "LCK.vbe.creat",                       .semantics = 'c', .format = 'i', .value = 10846808 },
    { .name = "LCK.vbe.destroy",                     .semantics = 'c', .format = 'i', .value = 9943719  },
    { .name = "LCK.vbe.locks",                       .semantics = 'c', .format = 'i', .value = 11350691 },
    { .name = "LCK.vbe.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 10004933 },
    { .name = "LCK.vbe.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 987515   },
    { .name = "LCK.vcapace.creat",                   .semantics = 'c', .format = 'i', .value = 9409866  },
    { .name = "LCK.vcapace.destroy",                 .semantics = 'c', .format = 'i', .value = 9455931  },
    { .name = "LCK.vcapace.locks",                   .semantics = 'c', .format = 'i', .value = 4070586  },
    { .name = "LCK.vcapace.dbg_busy",                .semantics = 'c', .format = 'i', .value = 317269   },
    { .name = "LCK.vcapace.dbg_try_fail",            .semantics = 'c', .format = 'i', .value = 5768701  },
    { .name = "LCK.vcashut.creat",                   .semantics = 'c', .format = 'i', .value = 152226   },
    { .name = "LCK.vcashut.destroy",                 .semantics = 'c', .format = 'i', .value = 15496984 },
    { .name = "LCK.vcashut.locks",                   .semantics = 'c', .format = 'i', .value = 10090266 },
    { .name = "LCK.vcashut.dbg_busy",                .semantics = 'c', .format = 'i', .value = 12929963 },
    { .name = "LCK.vcashut.dbg_try_fail",            .semantics = 'c', .format = 'i', .value = 14884695 },
    { .name = "LCK.vcl.creat",                       .semantics = 'c', .format = 'i', .value = 15657714 },
    { .name = "LCK.vcl.destroy",                     .semantics = 'c', .format = 'i', .value = 2903543  },
    { .name = "LCK.vcl.locks",                       .semantics = 'c', .format = 'i', .value = 7515882  },
    { .name = "LCK.vcl.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 8182595  },
    { .name = "LCK.vcl.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 13341766 },
    { .name = "LCK.vxid.creat",                      .semantics = 'c', .format = 'i', .value = 10720795 },
    { .name = "LCK.vxid.destroy",                    .semantics = 'c', .format = 'i', .value = 16201448 },
    { .name = "LCK.vxid.locks",                      .semantics = 'c', .format = 'i', .value = 2606112  },
    { .name = "LCK.vxid.dbg_busy",                   .semantics = 'c', .format = 'i', .value = 4913863  },
    { .name = "LCK.vxid.dbg_try_fail",               .semantics = 'c', .format = 'i', .value = 14800931 },
    { .name = "LCK.waiter.creat",                    .semantics = 'c', .format = 'i', .value = 6140929  },
    { .name = "LCK.waiter.destroy",                  .semantics = 'c', .format = 'i', .value = 15089952 },
    { .name = "LCK.waiter.locks",                    .semantics = 'c', .format = 'i', .value = 12543280 },
    { .name = "LCK.waiter.dbg_busy",                 .semantics = 'c', .format = 'i', .value = 7982693  },
    { .name = "LCK.waiter.dbg_try_fail",             .semantics = 'c', .format = 'i', .value = 4579963  },
    { .name = "LCK.wq.creat",                        .semantics = 'c', .format = 'i', .value = 15881992 },
    { .name = "LCK.wq.destroy",                      .semantics = 'c', .format = 'i', .value = 2052286  },
    { .name = "LCK.wq.locks",                        .semantics = 'c', .format = 'i', .value = 14523683 },
    { .name = "LCK.wq.dbg_busy",                     .semantics = 'c', .format = 'i', .value = 10455468 },
    { .name = "LCK.wq.dbg_try_fail",                 .semantics = 'c', .format = 'i', .value = 12057220 },
    { .name = "LCK.wstat.creat",                     .semantics = 'c', .format = 'i', .value = 15511199 },
    { .name = "LCK.wstat.destroy",                   .semantics = 'c', .format = 'i', .value = 3088118  },
    { .name = "LCK.wstat.locks",                     .semantics = 'c', .format = 'i', .value = 4735935  },
    { .name = "LCK.wstat.dbg_busy",                  .semantics = 'c', .format = 'i', .value = 2804569  },
    { .name = "LCK.wstat.dbg_try_fail",              .semantics = 'c', .format = 'i', .value = 3405388  },
    { .name = "MEMPOOL.busyobj.live",                .semantics = 'g', .format = 'i', .value = 10504636 },
    { .name = "MEMPOOL.busyobj.pool",                .semantics = 'g', .format = 'i', .value = 2956796  },
    { .name = "MEMPOOL.busyobj.sz_wanted",           .semantics = 'g', .format = 'B', .value = 2125157  },
    { .name = "MEMPOOL.busyobj.sz_actual",           .semantics = 'g', .format = 'B', .value = 3817687  },
    { .name = "MEMPOOL.busyobj.allocs",              .semantics = 'c', .format = 'i', .value = 15886759 },
    { .name = "MEMPOOL.busyobj.frees",               .semantics = 'c', .format = 'i', .value = 232637   },
    { .name = "MEMPOOL.busyobj.recycle",             .semantics = 'c', .format = 'i', .value = 2698186  },
    { .name = "MEMPOOL.busyobj.timeout",             .semantics = 'c', .format = 'i', .value = 2013086  },
    { .name = "MEMPOOL.busyobj.toosmall",            .semantics = 'c', .format = 'i', .value = 7748520  },
    { .name = "MEMPOOL.busyobj.surplus",             .semantics = 'c', .format = 'i', .value = 10880781 },
    { .name = "MEMPOOL.busyobj.randry",              .semantics = 'c', .format = 'i', .value = 15354853 },
    { .name = "VCP.ref_hit",                         .semantics = 'c', .format = 'i', .value = 1692099  },
    { .name = "VCP.ref_miss",                        .semantics = 'c', .format = 'i', .value = 10305014 },
    { .name = "MEMPOOL.req0.live",                   .semantics = 'g', .format = 'i', .value = 1183749  },
    { .name = "MEMPOOL.req0.pool",                   .semantics = 'g', .format = 'i', .value = 6605963  },
    { .name = "MEMPOOL.req0.sz_wanted",              .semantics = 'g', .format = 'B', .value = 8328729  },
    { .name = "MEMPOOL.req0.sz_actual",              .semantics = 'g', .format = 'B', .value = 7324679  },
    { .name = "MEMPOOL.req0.allocs",                 .semantics = 'c', .format = 'i', .value = 4918700  },
    { .name = "MEMPOOL.req0.frees",                  .semantics = 'c', .format = 'i', .value = 4094793  },
    { .name = "MEMPOOL.req0.recycle",                .semantics = 'c', .format = 'i', .value = 15307373 },
    { .name = "MEMPOOL.req0.timeout",                .semantics = 'c', .format = 'i', .value = 9498664  },
    { .name = "MEMPOOL.req0.toosmall",               .semantics = 'c', .format = 'i', .value = 3199569  },
    { .name = "MEMPOOL.req0.surplus",                .semantics = 'c', .format = 'i', .value = 582443   },
    { .name = "MEMPOOL.req0.randry",                 .semantics = 'c', .format = 'i', .value = 7245131  },
    { .name = "MEMPOOL.sess0.live",                  .semantics = 'g', .format = 'i', .value = 13655037 },
    { .name = "MEMPOOL.sess0.pool",                  .semantics = 'g', .format = 'i', .value = 12639663 },
    { .name = "MEMPOOL.sess0.sz_wanted",             .semantics = 'g', .format = 'B', .value = 5979114  },
    { .name = "MEMPOOL.sess0.sz_actual",             .semantics = 'g', .format = 'B', .value = 16743156 },
    { .name = "MEMPOOL.sess0.allocs",                .semantics = 'c', .format = 'i', .value = 598383   },
    { .name = "MEMPOOL.sess0.frees",                 .semantics = 'c', .format = 'i', .value = 8783684  },
    { .name = "MEMPOOL.sess0.recycle",               .semantics = 'c', .format = 'i', .value = 3371329  },
    { .name = "MEMPOOL.sess0.timeout",               .semantics = 'c', .format = 'i', .value = 11103020 },
    { .name = "MEMPOOL.sess0.toosmall",              .semantics = 'c', .format = 'i', .value = 11740480 },
    { .name = "MEMPOOL.sess0.surplus",               .semantics = 'c', .format = 'i', .value = 5496486  },
    { .name = "MEMPOOL.sess0.randry",                .semantics = 'c', .format = 'i', .value = 14920707 },
    { .name = "WAITER.pool0.conns",                  .semantics = 'g', .format = 'i', .value = 10850024 },
    { .name = "WAITER.pool0.remclose",               .semantics = 'c', .format = 'i', .value = 5729123  },
    { .name = "WAITER.pool0.timeout",                .semantics = 'c', .format = 'i', .value = 841677   },
    { .name = "WAITER.pool0.action",                 .semantics = 'c', .format = 'i', .value = 12863110 },
    { .name = "LCK.sma.creat",                       .semantics = 'c', .format = 'i', .value = 13477643 },
    { .name = "LCK.sma.destroy",                     .semantics = 'c', .format = 'i', .value = 11722459 },
    { .name = "LCK.sma.locks",                       .semantics = 'c', .format = 'i', .value = 11440748 },
    { .name = "LCK.sma.dbg_busy",                    .semantics = 'c', .format = 'i', .value = 15169742 },
    { .name = "LCK.sma.dbg_try_fail",                .semantics = 'c', .format = 'i', .value = 5250257  },
    { .name = "SMA.s0.c_req",                        .semantics = 'c', .format = 'i', .value = 12624497 },
    { .name = "SMA.s0.c_fail",                       .semantics = 'c', .format = 'i', .value = 4998490  },
    { .name = "SMA.s0.c_bytes",                      .semantics = 'c', .format = 'B', .value = 13578986 },
    { .name = "SMA.s0.c_freed",                      .semantics = 'c', .format = 'B', .value = 3171961  },
    { .name = "SMA.s0.g_alloc",                      .semantics = 'g', .format = 'i', .value = 9917190  },
    { .name = "SMA.s0.g_bytes",                      .semantics = 'g', .format = 'B', .value = 896564   },
    { .name = "SMA.s0.g_space",                      .semantics = 'g', .format = 'B', .value = 1702118  },
    { .name = "SMA.Transient.c_req",                 .semantics = 'c', .format = 'i', .value = 2638638  },
    { .name = "SMA.Transient.c_fail",                .semantics = 'c', .format = 'i', .value = 4096134  },
    { .name = "SMA.Transient.c_bytes",               .semantics = 'c', .format = 'B', .value = 2284562  },
    { .name = "SMA.Transient.c_freed",               .semantics = 'c', .format = 'B', .value = 9883770  },
    { .name = "SMA.Transient.g_alloc",               .semantics = 'g', .format = 'i', .value = 973955   },
    { .name = "SMA.Transient.g_bytes",               .semantics = 'g', .format = 'B', .value = 14924225 },
    { .name = "SMA.Transient.g_space",               .semantics = 'g', .format = 'B', .value = 15862885 },
    { .name = "MEMPOOL.req1.live",                   .semantics = 'g', .format = 'i', .value = 939896   },
    { .name = "MEMPOOL.req1.pool",                   .semantics = 'g', .format = 'i', .value = 15522609 },
    { .name = "MEMPOOL.req1.sz_wanted",              .semantics = 'g', .format = 'B', .value = 7869353  },
    { .name = "MEMPOOL.req1.sz_actual",              .semantics = 'g', .format = 'B', .value = 4311225  },
    { .name = "MEMPOOL.req1.allocs",                 .semantics = 'c', .format = 'i', .value = 9848413  },
    { .name = "MEMPOOL.req1.frees",                  .semantics = 'c', .format = 'i', .value = 2832617  },
    { .name = "MEMPOOL.req1.recycle",                .semantics = 'c', .format = 'i', .value = 9807711  },
    { .name = "MEMPOOL.req1.timeout",                .semantics = 'c', .format = 'i', .value = 7991905  },
    { .name = "MEMPOOL.req1.toosmall",               .semantics = 'c', .format = 'i', .value = 13682641 },
    { .name = "MEMPOOL.req1.surplus",                .semantics = 'c', .format = 'i', .value = 15536835 },
    { .name = "MEMPOOL.req1.randry",                 .semantics = 'c', .format = 'i', .value = 8833582  },
    { .name = "MEMPOOL.sess1.live",                  .semantics = 'g', .format = 'i', .value = 9768536  },
    { .name = "MEMPOOL.sess1.pool",                  .semantics = 'g', .format = 'i', .value = 12237262 },
    { .name = "MEMPOOL.sess1.sz_wanted",             .semantics = 'g', .format = 'B', .value = 3778825  },
    { .name = "MEMPOOL.sess1.sz_actual",             .semantics = 'g', .format = 'B', .value = 4432068  },
    { .name = "MEMPOOL.sess1.allocs",                .semantics = 'c', .format = 'i', .value = 10629789 },
    { .name = "MEMPOOL.sess1.frees",                 .semantics = 'c', .format = 'i', .value = 9029082  },
    { .name = "MEMPOOL.sess1.recycle",               .semantics = 'c', .format = 'i', .value = 279350   },
    { .name = "MEMPOOL.sess1.timeout",               .semantics = 'c', .format = 'i', .value = 15628279 },
    { .name = "MEMPOOL.sess1.toosmall",              .semantics = 'c', .format = 'i', .value = 5830853  },
    { .name = "MEMPOOL.sess1.surplus",               .semantics = 'c', .format = 'i', .value = 3451311  },
    { .name = "MEMPOOL.sess1.randry",                .semantics = 'c', .format = 'i', .value = 8768253  },
    { .name = "WAITER.pool1.conns",                  .semantics = 'g', .format = 'i', .value = 6727417  },
    { .name = "WAITER.pool1.remclose",               .semantics = 'c', .format = 'i', .value = 5153429  },
    { .name = "WAITER.pool1.timeout",                .semantics = 'c', .format = 'i', .value = 11406892 },
    { .name = "WAITER.pool1.action",                 .semantics = 'c', .format = 'i', .value = 10823551 },
    { .name = "VBE.boot.default.happy",              .semantics = 'b', .format = 'b', .value = 7437991  },
    { .name = "VBE.boot.default.bereq_hdrbytes",     .semantics = 'c', .format = 'B', .value = 4513446  },
    { .name = "VBE.boot.default.bereq_bodybytes",    .semantics = 'c', .format = 'B', .value = 11797507 },
    { .name = "VBE.boot.default.beresp_hdrbytes",    .semantics = 'c', .format = 'B', .value = 5585001  },
    { .name = "VBE.boot.default.beresp_bodybytes",   .semantics = 'c', .format = 'B', .value = 3599115  },
    { .name = "VBE.boot.default.pipe_hdrbytes",      .semantics = 'c', .format = 'B', .value = 12737403 },
    { .name = "VBE.boot.default.pipe_out",           .semantics = 'c', .format = 'B', .value = 4330395  },
    { .name = "VBE.boot.default.pipe_in",            .semantics = 'c', .format = 'B', .value = 11468468 },
    { .name = "VBE.boot.default.conn",               .semantics = 'g', .format = 'i', .value = 271413   },
    { .name = "VBE.boot.default.req",                .semantics = 'c', .format = 'i', .value = 14178809 },
    { .name = "VBE.boot.default.unhealthy",          .semantics = 'c', .format = 'i', .value = 14301086 },
    { .name = "VBE.boot.default.busy",               .semantics = 'c', .format = 'i', .value = 10079124 },
    { .name = "VBE.boot.default.fail",               .semantics = 'c', .format = 'i', .value = 5393498  },
    { .name = "VBE.boot.default.fail_eacces",        .semantics = 'c', .format = 'i', .value = 11206511 },
    { .name = "VBE.boot.default.fail_eaddrnotavail", .semantics = 'c', .format = 'i', .value = 8838743  },
    { .name = "VBE.boot.default.fail_econnrefused",  .semantics = 'c', .format = 'i', .value = 14227080 },
    { .name = "VBE.boot.default.fail_enetunreach",   .semantics = 'c', .format = 'i', .value = 4197832  },
    { .name = "VBE.boot.default.fail_etimedout",     .semantics = 'c', .format = 'i', .value = 4298790  },
    { .name = "VBE.boot.default.fail_other",         .semantics = 'c', .format = 'i', .value = 1228690  },
    { .name = "VBE.boot.default.helddown",           .semantics = 'c', .format = 'i', .value = 8629900  },
    { .name = NULL,                                  .semantics = 0,   .format = 0,   .value = 0        }
};

struct vsm *VSM_New(void)
{
    return (struct vsm *)calloc(1, sizeof(void *));
}

struct vsc *VSC_New(void)
{
    return (struct vsc *)calloc(1, sizeof(void *));
}

int VSM_Arg(struct vsm *vsmd, char flag, const char *arg)
{
    (void)vsmd;
    (void)flag;
    (void)arg;

    return 0;
}

void VSC_Destroy(struct vsc **vscd, struct vsm *vsmd)
{
    (void)vsmd;
    free(*vscd);
    *vscd = NULL;
}

void VSM_Destroy(struct vsm **vsmd)
{
    free(*vsmd);
    *vsmd = NULL;
}


int VSM_Attach(struct vsm *vsmd, int progress)
{
    (void)vsmd;
    (void)progress;

    return 0;
}

unsigned VSM_Status(struct vsm *vsmd)
{
    (void)vsmd;
    return VSM_MGT_RUNNING;
}

int VSC_Iter(struct vsc *vsdc, struct vsm *vsmd, VSC_iter_f *iter, void *priv)
{
    (void)vsdc;
    (void)vsmd;

    for (size_t i = 0; vsh_metrics[i].name != NULL; i++) {
        struct VSC_point pt = {0};

        pt.name = vsh_metrics[i].name;
        pt.semantics = vsh_metrics[i].semantics;
        pt.format = vsh_metrics[i].format;
        pt.ptr = &vsh_metrics[i].value;

        iter(priv, &pt);
    }

    return 0;
}

DEF_TEST(test01)
{
    char *config = "instance local { collect all }";
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci, "src/plugins/varnish/test01/expect.txt"));

    config_free(ci);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    END_TEST;
}
