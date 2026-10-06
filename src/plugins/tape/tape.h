/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

enum {
    FAM_TAPE_IN_FLIGHT_REQUESTS,
    FAM_TAPE_OTHER_OPS,
    FAM_TAPE_READ_BYTES,
    FAM_TAPE_READ_OPS,
    FAM_TAPE_READ_TIME,
    FAM_TAPE_READ_WEIGHTED_TIME,
    FAM_TAPE_WRITE_BYTES,
    FAM_TAPE_WRITE_OPS,
    FAM_TAPE_WRITE_TIME,
    FAM_TAPE_WRITE_WEIGHTED_TIME,
    FAM_TAPE_RESIDUAL,
    FAM_TAPE_IO_TIME,
    FAM_TAPE_WAIT_TIME,
    FAM_TAPE_MAX
};
