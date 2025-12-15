/* SPDX-License-Identifier: GPL-2.0-only                                   */
/* SPDX-FileCopyrightText: Copyright (C) 2007,2008 Florian octo Forster    */
/* SPDX-FileCopyrightText: Copyright (C) 2008 Michael Stapelberg           */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín        */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>       */
/* SPDX-FileContributor: Michael Stapelberg <michael+git at stapelberg.de> */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>       */

#pragma once

void conn_submit_all(const char *tcp_state[], int tcp_state_min, int tcp_state_max);

void conn_free_port_entries(void);

int conn_handle_ports(struct sockaddr *local, struct sockaddr *remote, uint8_t state,
                      int tcp_state_min, int tcp_state_max);
