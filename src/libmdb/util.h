/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

int64_t mql_parse_duration (char *str);

char *mql_unquote(char *str);

bool mql_islabel(char *str);

bool mql_ismetric(char *str);
