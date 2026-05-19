/* SPDX-License-Identifier: GPL-2.0-only                              */
/* SPDX-FileCopyrightText: Copyright (C) Tobias Waldekranz            */
/* SPDX-FileContributor: Tobias Waldekranz <tobias at waldekranz.com> */

/* From https://github.com/wkz/ply */

#pragma once

#include "arch.h"
#include "buffer.h"
#include "func.h"
#include "ir.h"
#include "node.h"
#include "provider.h"
#include "sym.h"
#include "type.h"

#include "kallsyms.h"
#include "perf_event.h"
#include "printxf.h"
#include "syscall.h"
#include "utils.h"

void built_in_init(void);

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof(a[0]))

