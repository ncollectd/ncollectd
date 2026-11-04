// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libutils/common.h"

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

int test_dump_file(char *base_path, char *file, int fdw)
{
    char file_path[PATH_MAX];
    ssnprintf(file_path, sizeof(file_path), "%s/%s", base_path, file);

    int fdr = open(file_path, O_RDONLY);
    if (fdr < 0) {
        fprintf(stderr, "Cannot open '%s'.\n", file_path);
        return -1;
    }

    char buffer[8192];
    ssize_t size = 0;
    while ((size = read(fdr, buffer, sizeof(buffer))) > 0) {
        int status = write(fdw, buffer, size);
        if (status < 0)
            fprintf(stderr, "Failed to write.\n");
    }

    close(fdr);

    return 0;
}

int test_slow_dump_file(char *base_path, char *file, int fdw)
{
    char file_path[PATH_MAX];
    ssnprintf(file_path, sizeof(file_path), "%s/%s", base_path, file);

    int fdr = open(file_path, O_RDONLY);
    if (fdr < 0) {
        fprintf(stderr, "Cannot open '%s'.\n", file_path);
        return -1;
    }

    int val = 1;
	setsockopt(fdw, SOL_TCP, TCP_NODELAY, &val, sizeof(val));

    char buffer[8192];
    ssize_t size = 0;
    while ((size = read(fdr, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < size; i++) {
            int status = write(fdw, buffer+i, 1);
            if (status < 0)
                fprintf(stderr, "Failed to write.\n");
        }
    }

    close(fdr);

    return 0;
}
