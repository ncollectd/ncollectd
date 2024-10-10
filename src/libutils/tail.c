// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007-2008 C-Ware, Inc.
// SPDX-FileCopyrightText: Copyright (C) 2008-2013 Florian Forster
// SPDX-FileCopyrightText: Copyright (C) 2013 Kris Nielander
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Kris Nielander <nielander at fox-it.com>
// SPDX-FileContributor: Florian Forster <octo at collectd.org>
// SPDX-FileContributor: Luke Heberling <lukeh at c-ware.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/tail.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

void tail_free(tail_t *tail)
{
    if (tail == NULL)
        return;

    if (tail->fh != NULL)
        fclose(tail->fh);

    free(tail->file);

    free(tail);
    return;
}

void tail_reset(tail_t *tail)
{
    if (tail == NULL)
        return;

    if (tail->fh != NULL)
        fclose(tail->fh);

    free(tail->file);
    return;
}

tail_t *tail_alloc(char *file, bool force_rewind)
{
    tail_t *tail = calloc(1, sizeof(*tail));
    if (tail == NULL) {
        ERROR("calloc failed.");
        return NULL;
    }

    tail->file = strdup(file);
    if (tail->file == NULL) {
        ERROR("strdup failed.");
        free(tail);
        return NULL;
    }

    tail->force_rewind = force_rewind;

    return tail;
}

int tail_close(tail_t *tail)
{
    if (tail->fh == NULL)
        return 0;

    fclose(tail->fh);
    tail->fh = NULL;
    return 0;
}

int tail_reopen(tail_t *tail)
{
    struct stat stat_buf = {0};
    /* coverity[TOCTOU] */
    int status = stat(tail->file, &stat_buf);
    if (status != 0) {
        ERROR("Stat '%s' failed: %s", tail->file, STRERRNO);
        return -1;
    }

    /* The file is already open.. */
    if ((tail->fh != NULL) && (stat_buf.st_ino == tail->stat.st_ino)) {
        /* Seek to the beginning if file was truncated */
        if (stat_buf.st_size < tail->stat.st_size) {
            PLUGIN_INFO("File '%s' was truncated.", tail->file);
            status = fseek(tail->fh, 0, SEEK_SET);
            if (status != 0) {
                ERROR("fseek '%s' failed: %s", tail->file, STRERRNO);
                fclose(tail->fh);
                tail->fh = NULL;
                return -1;
            }
        }
        memcpy(&tail->stat, &stat_buf, sizeof(struct stat));
        return 1;
    }

    /* Unless flag for rewinding to the start is set, seek to the end
     * if we re-open the same file again or the file opened is the first at all
     * or the first after an error */
    int seek_end = 0;
    if ((tail->stat.st_ino == 0) || (tail->stat.st_ino == stat_buf.st_ino))
        seek_end = !tail->force_rewind;

    FILE *fh = fopen(tail->file, "r");
    if (fh == NULL) {
        ERROR("Cannot open '%s': %s", tail->file, STRERRNO);
        return -1;
    }

    if (seek_end != 0) {
        status = fseek(fh, 0, SEEK_END);
        if (status != 0) {
            ERROR("fseek '%s' failed: %s", tail->file, STRERRNO);
            fclose(fh);
            return -1;
        }
    }

    if (tail->fh != NULL)
        fclose(tail->fh);
    tail->fh = fh;
    memcpy(&tail->stat, &stat_buf, sizeof(struct stat));

    return 0;
}

int tail_readline(tail_t *tail, char *buf, size_t buflen)
{
    if (tail->fh == NULL) {
        int status = tail_reopen(tail);
        if (status < 0)
            return status;
    }
    assert(tail->fh != NULL);

    /* Try to read from the filehandle. If that succeeds, everything appears to
     * be fine and we can return. */
    clearerr(tail->fh);
    if (fgets(buf, buflen, tail->fh) != NULL) {
        buf[buflen - 1] = '\0';
        return 0;
    }

    /* Check if we encountered an error */
    if (ferror(tail->fh) != 0) {
        /* Jupp, error. Force `cu_tail_reopen' to reopen the file.. */
        fclose(tail->fh);
        tail->fh = NULL;
    }
    /* else: eof -> check if the file was moved away and reopen the new file if
     * so.. */

    int status = tail_reopen(tail);
    /* error -> return with error */
    if (status < 0) {
        return status;
    /* file end reached and file not reopened -> nothing more to read */
    } else if (status > 0) {
        buf[0] = 0;
        return 0;
    }

    /* If we get here: file was re-opened and there may be more to read.. Let's
     * try again. */
    if (fgets(buf, buflen, tail->fh) != NULL) {
        buf[buflen - 1] = '\0';
        return 0;
    }

    if (ferror(tail->fh) != 0) {
        WARNING("fgets '%s' returned an error: %s", tail->file, STRERRNO);
        fclose(tail->fh);
        tail->fh = NULL;
        return -1;
    }

    /* EOf, well, apparently the new file is empty.. */
    buf[0] = 0;
    return 0;
}
