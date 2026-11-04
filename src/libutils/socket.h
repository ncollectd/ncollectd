/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

#include "libutils/time.h"

int socket_listen_unix_stream(const char *file, int backlog, char const *group, int perms,
                              bool delete, cdtime_t timeout);

int socket_connect_unix_stream(const char *path, cdtime_t timeout);

int socket_connect_unix_dgram(const char *localpath, const char *path, cdtime_t timeout);

int socket_listen_tcp(const char *host, int port, int addrfamily, int backlog, bool reuseaddr);

int socket_connect_udp(const char *host, int port, int ttl);

int socket_connect_tcp(const char *host, int port, int keep_idle, int keep_int);

int socket_get_port(int sfd);

int socket_setfl(int sfd, int setfl);

int socket_poll_connect_tcp(const char *host, int port, int keep_idle, int keep_int,
                            cdtime_t *timeout);

int socket_poll_connect_unix_stream(const char *path, cdtime_t *timeout);

ssize_t socket_poll_read(int fd, void *buf, size_t len, cdtime_t *timeout);

ssize_t socket_poll_write(int fd, const void *buf, size_t len, cdtime_t *timeout);
