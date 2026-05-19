/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#define __ply_built_in __attribute__((    \
     section("built_ins"),        \
     aligned(__alignof__(struct func))    \
))

extern const struct func __start_built_ins;
extern const struct func __stop_built_ins;

void built_in_register(struct func *func);

void aggregation_init(void);
void buffer_init(void);
void flow_init(void);
void math_init(void);
void memory_init(void);
void print_init(void);
void proc_init(void);

