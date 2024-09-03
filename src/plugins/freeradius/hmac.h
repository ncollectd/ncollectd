/* SPDX-License-Identifier: BSD-2-Clause                         */
/* SPDX-FileCopyrightText: Copyright (C) 2016  Martin Belanger   */
/* SPDX-FileContributor: Martin Belanger <nitram_67@hotmail.com> */

#pragma once

void hmac_md5(uint8_t *data, size_t data_len, uint8_t *key,  size_t key_len, uint8_t *digest);
