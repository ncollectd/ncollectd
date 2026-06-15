// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libutils/common.h"

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
