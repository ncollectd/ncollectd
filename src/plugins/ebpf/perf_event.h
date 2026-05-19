/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#define TRACEPATH "/sys/kernel/debug/tracing/"

struct ply_probe;

int perf_event_attach(struct ply_probe *pb, const char *name,
              int task_mode);
int perf_event_attach_raw(struct ply_probe *pb, int type, unsigned long config,
              unsigned long long period, int task_mode);
int perf_event_attach_profile(struct ply_probe *pb, int cpu,
              unsigned long long freq);

int perf_event_enable (int group_fd);
int perf_event_disable(int group_fd);

