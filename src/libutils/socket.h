/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/time.h"

int socket_listen_unix_stream(const char *file, int backlog, char const *group, int perms,
                              bool delete, cdtime_t timeout);

int socket_connect_unix_stream(const char *path, cdtime_t timeout);

int socket_connect_unix_dgram(const char *localpath, const char *path, cdtime_t timeout);

int socket_listen_tcp(const char *host, short port, int addrfamily, int backlog, bool reuseaddr);

int socket_connect_udp(const char *host, short port, int ttl);

int socket_connect_tcp(const char *host, short port, int keep_idle, int keep_int);
