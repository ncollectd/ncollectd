/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libmetric/notification.h"

notification_t *notification_json_parse(const char *data, size_t len);

int notification_json(strbuf_t *buf, notification_t const *n);
