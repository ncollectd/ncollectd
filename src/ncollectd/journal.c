// SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause
// SPDX-FileCopyrightText: Copyright (c) 2005-2008, Message Systems, Inc.
// SPDX-FileCopyrightText: Copyright (c) 2016, Circonus, Inc.

/* From https://github.com/omniti-labs/jlog */

#include "ncollectd.h"
#include "plugin_internal.h"
#include "libutils/avltree.h"
#include "libcompress/lz4.h"

#include <sys/uio.h>
#include <sys/mman.h>
#include <pthread.h>
// #include <assert.h>

#include "journal.h"

// static pthread_mutex_t journal_files_lock = PTHREAD_MUTEX_INITIALIZER;

struct _journal_subs {
    char **subs;
    int used;
    int allocd;
};

static journal_file_t *journal_open_indexer(journal_ctx_t *ctx, uint32_t log);

static journal_ctx_t *journal_ctx_new(journal_t *j);

#define DEFAULT_FILE_MODE 0640
#define DEFAULT_UNIT_LIMIT (4*1024*1024) /* 4 Megabytes */
#define DEFAULT_HDR_MAGIC 0x663A7318
#define DEFAULT_HDR_MAGIC_COMPRESSION 0x15106A00
#define DEFAULT_SAFETY JOURNAL_ALMOST_SAFE
#define DEFAULT_READ_MESSAGE_TYPE JOURNAL_READ_METHOD_MMAP
#define INDEX_EXT ".idx"
#define MAXLOGPATHLEN (MAXPATHLEN - (8+sizeof(INDEX_EXT)))
#define IFS_CH '/'

#define BUFFERED_INDICES 1024
#define PRE_COMMIT_BUFFER_SIZE_DEFAULT 0
#define IS_COMPRESS_MAGIC_HDR(hdr) ((hdr & DEFAULT_HDR_MAGIC_COMPRESSION) == DEFAULT_HDR_MAGIC_COMPRESSION)
#define IS_COMPRESS_MAGIC(ctx) IS_COMPRESS_MAGIC_HDR((ctx)->meta->hdr_magic)

static const char journal_hexchars[] = "0123456789abcdef";

#define SYS_FAIL_EX(a, dowarn) do { \
    if (ctx) { \
      ctx->last_error = (a); \
      ctx->last_errno = errno; \
      ERROR("error: %d (%s) errno: %d (%s)", \
             ctx->last_error, journal_ctx_err_string(ctx), \
             ctx->last_errno, STRERRNO); \
    } \
    goto finish; \
} while (0)

#define SYS_FAIL(a) SYS_FAIL_EX(a, 1)

static void journal_set_data_file(journal_ctx_t *ctx, char *file, uint32_t logid)
{
    size_t len = strlen(ctx->path);
    memcpy(file, ctx->path, len);
    file[len] = IFS_CH;

    char *fileid = file + len + 1;

    int i;
    for (i=0; i<8; i++) {
      fileid[i] = journal_hexchars[((logid) >> (32 - ((i+1)*4))) & 0xf];
    }

    fileid[i] = '\0';
}

static int journal_compress(const char *source, const size_t source_bytes, char **dest,
                            size_t *dest_bytes)
{
    size_t required = LZ4_compressBound(source_bytes);

    if (*dest_bytes < required) {
        /* incoming buffer not large enough, must allocate */
        *dest = malloc(required);
        if (*dest == NULL)
            ERROR("journal_compress: malloc failed");
    }

    int rv = LZ4_compress_default(source, *dest, source_bytes, required);
    if (rv > 0) {
        DEBUG("Compressed %zu bytes into %d bytes.", source_bytes, rv);
        *dest_bytes = rv;
        return 0;
    }

    return rv;
}

static int journal_decompress(const char *source, const size_t source_bytes, char *dest,
                              size_t dest_bytes)
{
    if (unlikely(dest == NULL))
        return -1;

    int rv = LZ4_decompress_safe(source, dest, source_bytes, dest_bytes);
    if (rv >= 0) {
        DEBUG("Compressed %zu bytes into %d bytes.", source_bytes, rv);
        return 0;
    }

    return rv;
}

const char *journal_err_string(int err)
{
    switch (err) {
    case JOURNAL_ERR_SUCCESS:
        return "JOURNAL_ERR_SUCCESS";
    case JOURNAL_ERR_ILLEGAL_INIT:
        return "JOURNAL_ERR_ILLEGAL_INIT";
    case JOURNAL_ERR_ILLEGAL_OPEN:
        return "JOURNAL_ERR_ILLEGAL_OPEN";
    case JOURNAL_ERR_OPEN:
        return "JOURNAL_ERR_OPEN";
    case JOURNAL_ERR_NOTDIR:
        return "JOURNAL_ERR_NOTDIR";
    case JOURNAL_ERR_CREATE_PATHLEN:
        return "JOURNAL_ERR_CREATE_PATHLEN";
    case JOURNAL_ERR_CREATE_EXISTS:
        return "JOURNAL_ERR_CREATE_EXISTS";
    case JOURNAL_ERR_CREATE_MKDIR:
        return "JOURNAL_ERR_CREATE_MKDIR";
    case JOURNAL_ERR_CREATE_META:
        return "JOURNAL_ERR_CREATE_META";
    case JOURNAL_ERR_LOCK:
        return "JOURNAL_ERR_LOCK";
    case JOURNAL_ERR_IDX_OPEN:
        return "JOURNAL_ERR_IDX_OPEN";
    case JOURNAL_ERR_IDX_SEEK:
        return "JOURNAL_ERR_IDX_SEEK";
    case JOURNAL_ERR_IDX_CORRUPT:
        return "JOURNAL_ERR_IDX_CORRUPT";
    case JOURNAL_ERR_IDX_WRITE:
        return "JOURNAL_ERR_IDX_WRITE";
    case JOURNAL_ERR_IDX_READ:
        return "JOURNAL_ERR_IDX_READ";
    case JOURNAL_ERR_FILE_OPEN:
        return "JOURNAL_ERR_FILE_OPEN";
    case JOURNAL_ERR_FILE_SEEK:
        return "JOURNAL_ERR_FILE_SEEK";
    case JOURNAL_ERR_FILE_CORRUPT:
        return "JOURNAL_ERR_FILE_CORRUPT";
    case JOURNAL_ERR_FILE_READ:
        return "JOURNAL_ERR_FILE_READ";
    case JOURNAL_ERR_FILE_WRITE:
        return "JOURNAL_ERR_FILE_WRITE";
    case JOURNAL_ERR_META_OPEN:
        return "JOURNAL_ERR_META_OPEN";
    case JOURNAL_ERR_ILLEGAL_WRITE:
        return "JOURNAL_ERR_ILLEGAL_WRITE";
    case JOURNAL_ERR_ILLEGAL_CHECKPOINT:
        return "JOURNAL_ERR_ILLEGAL_CHECKPOINT";
    case JOURNAL_ERR_INVALID_SUBSCRIBER:
        return "JOURNAL_ERR_INVALID_SUBSCRIBER";
    case JOURNAL_ERR_ILLEGAL_LOGID:
        return "JOURNAL_ERR_ILLEGAL_LOGID";
    case JOURNAL_ERR_SUBSCRIBER_EXISTS:
        return "JOURNAL_ERR_SUBSCRIBER_EXISTS";
    case JOURNAL_ERR_CHECKPOINT:
        return "JOURNAL_ERR_CHECKPOINT";
    case JOURNAL_ERR_NOT_SUPPORTED:
        return "JOURNAL_ERR_NOT_SUPPORTED";
    case JOURNAL_ERR_CLOSE_LOGID:
        return "JOURNAL_ERR_CLOSE_LOGID";
    default:
        return "Unknown";
    }
}

const char *journal_ctx_err_string(journal_ctx_t *ctx)
{
    return journal_err_string(ctx->last_error);
}

int journal_ctx_err(journal_ctx_t *ctx)
{
    return ctx->last_error;
}

int journal_ctx_errno(journal_ctx_t *ctx)
{
    return ctx->last_errno;
}

static bool is_datafile(const char *f, uint32_t *logid)
{
    int i;
    uint32_t l = 0;

    for (i=0; i<8; i++) {
        if ((f[i] >= '0' && f[i] <= '9') || (f[i] >= 'a' && f[i] <= 'f')) {
            l <<= 4;
            l |= (f[i] < 'a') ? (f[i] - '0') : (f[i] - 'a' + 10);
        } else {
            return false;
        }
    }

    if (f[i] != '\0')
        return false;

    if (logid)
        *logid = l;

    return true;
}



static c_avl_tree_t *journal_files;

static int journal_files_cmp(const void *a, const void *b)
{
    return memcmp((const journal_file_id_t *)a, (const journal_file_id_t *)b,
                   sizeof(journal_file_id_t));
}

static journal_file_t *journal_files_retrieve(journal_file_id_t *id)
{
    if (journal_files == NULL) {
        journal_files = c_avl_create(journal_files_cmp);
        if (journal_files == NULL)
            return NULL;
    }

    journal_file_t *f;
    int status = c_avl_get(journal_files, id, (void *)&f);
    if (status == 0)
        return f;

    return NULL;
}

static int journal_files_store(journal_file_t *f)
{
    if (journal_files == NULL) {
        journal_files = c_avl_create(journal_files_cmp);
        if (journal_files == NULL)
            return -1;
    }

    int status = c_avl_insert(journal_files, &f->id, f);
    if (status != 0)
        return 0;

    return 1;
}

static void journal_files_delete(journal_file_t *f)
{
    if (journal_files == NULL)
        return;

    c_avl_remove(journal_files, &f->id, NULL, NULL);
}

static journal_file_t *journal_file_open(journal_t *j, const char *path, int flags, int mode)
{
    struct stat sb;
    journal_file_id_t id;
    journal_file_t *f = NULL;
    int fd, realflags = O_RDWR, rv;

    if (flags & O_CREAT)
        realflags |= O_CREAT;
    if (flags & O_EXCL)
        realflags |= O_EXCL;

    if (pthread_mutex_lock(&j->lock) != 0)
        return NULL;

    while ((rv = stat(path, &sb)) == -1 && errno == EINTR);

    if (rv == 0) {
        if (!S_ISREG(sb.st_mode))
            goto out;
        memset(&id, 0, sizeof(id));
        id.st_dev = sb.st_dev;
        id.st_ino = sb.st_ino;
        journal_file_t *ff = journal_files_retrieve(&id);
        if (ff != NULL) {
            if (!(flags & O_EXCL)) {
                f = ff;
                f->refcnt++;
            } else {
                errno = EEXIST;
            }
            goto out;
        }
    }

    while ((fd = open(path, realflags, mode)) == -1 && errno == EINTR);
    if (fd == -1)
        goto out;

    while ((rv = fstat(fd, &sb)) == -1 && errno == EINTR);
    if (rv != 0) {
        while (close(fd) == -1 && errno == EINTR);
        goto out;
    }
    id.st_dev = sb.st_dev;
    id.st_ino = sb.st_ino;

    if (!(f = malloc(sizeof(journal_file_t)))) {
        while (close(fd) == -1 && errno == EINTR);
        goto out;
    }

    memset(f, 0, sizeof(journal_file_t));
    f->id = id;
    f->fd = fd;
    f->refcnt = 1;
    f->locked = 0;
    pthread_mutex_init(&(f->lock), NULL);
    if (!journal_files_store(f)) {
        while (close(f->fd) == -1 && errno == EINTR);
        free(f);
        f = NULL;
    }
out:
    pthread_mutex_unlock(&j->lock);
    return f;
}

static int journal_file_close(journal_t *j, journal_file_t *f)
{
    if (pthread_mutex_lock(&j->lock) != 0)
        return 0;

    if (--f->refcnt == 0) {
        journal_files_delete(f);
        while (close(f->fd) == -1 && errno == EINTR);
        pthread_mutex_destroy(&(f->lock));
        free(f);
    }

    pthread_mutex_unlock(&j->lock);

    return 1;
}

static int journal_file_lock(journal_file_t *f)
{
    if (pthread_mutex_lock(&(f->lock)) != 0) {
        ERROR("pthread_mutex_lock: %s", STRERRNO);
        return 0;
    }

    f->locked = 1;

    return 1;
}

static int journal_file_unlock(journal_file_t *f)
{
    if (!f->locked)
        return 0;

    f->locked = 0;

    pthread_mutex_unlock(&(f->lock));

    return 1;
}

static int journal_file_pread(journal_file_t *f, void *buf, size_t nbyte, off_t offset)
{
    while (nbyte > 0) {
        ssize_t rv = pread(f->fd, buf, nbyte, offset);
        if (rv == -1 && errno == EINTR)
            continue;
        if (rv <= 0)
            return 0;
        nbyte -= rv;
        offset += rv;
    }
    return 1;
}

static int journal_file_pwrite(journal_file_t *f, const void *buf, size_t nbyte, off_t offset)
{
    while (nbyte > 0) {
        ssize_t rv = pwrite(f->fd, buf, nbyte, offset);
        if (rv == -1 && errno == EINTR)
            continue;
        if (rv <= 0)
            return 0;
        nbyte -= rv;
        offset += rv;
    }
    return 1;
}

static int journal_file_pwritev_verify_return_value(journal_file_t *f, const struct iovec *vecs,
                                                    int iov_count, off_t offset, size_t expected_length)
{
    ssize_t rv = 0;
    while (1) {
#ifdef HAVE_PWRITEV
        rv = pwritev(f->fd, vecs, iov_count, offset);
#else
        if (lseek(f->fd, offset, SEEK_SET) < 0)
            return 0;
        rv = writev(f->fd, vecs, iov_count);
#endif
        if ((rv == -1) && (errno == EINTR))
            continue;
        if (rv <= 0)
            return 0;
        if ((rv > 0) && (rv != (ssize_t)expected_length))
            continue;
        break;
    }
    return 1;
}

#if 0
static int journal_file_pwritev(journal_file_t *f, const struct iovec *vecs, int iov_count, off_t offset)
{
    int i;
    size_t expected_length = 0;
    for (i=0; i < iov_count; i++) {
        expected_length += vecs[i].iov_len;
    }

    if (expected_length) {
        return journal_file_pwritev_verify_return_value(f, vecs, iov_count, offset, expected_length);
    }

    return 0;
}
#endif

static int journal_file_sync(journal_file_t *f)
{
    int rv;

#ifdef HAVE_FDATASYNC
    while ((rv = fdatasync(f->fd)) == -1 && errno == EINTR);
#else
    while ((rv = fsync(f->fd)) == -1 && errno == EINTR);
#endif
    if (rv == 0)
        return 1;
    return 0;
}

static int journal_file_map_rdwr(journal_file_t *f, void **base, size_t *len)
{
    struct stat sb;
    int flags = 0;

#ifdef MAP_SHARED
    flags = MAP_SHARED;
#endif
    if (fstat(f->fd, &sb) != 0)
        return 0;
    void *my_map = mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE, flags, f->fd, 0);
    if (my_map == MAP_FAILED)
        return 0;
    *base = my_map;
    *len = sb.st_size;
    return 1;
}

static int journal_file_map_read(journal_file_t *f, void **base, size_t *len)
{
    struct stat sb;
    int flags = 0;

#ifdef MAP_SHARED
    flags = MAP_SHARED;
#endif
    if (fstat(f->fd, &sb) != 0)
        return 0;
    void *my_map = mmap(NULL, sb.st_size, PROT_READ, flags, f->fd, 0);
    if (my_map == MAP_FAILED)
        return 0;
    *base = my_map;
    *len = sb.st_size;
    return 1;
}

static off_t journal_file_size(journal_file_t *f)
{
    struct stat sb;
    int rv;
    while ((rv = fstat(f->fd, &sb)) == -1 && errno == EINTR);
    if (rv != 0)
        return -1;
    return sb.st_size;
}

static int journal_file_truncate(journal_file_t *f, off_t len)
{
    int rv;
    while ((rv = ftruncate(f->fd, len)) == -1 && errno == EINTR);
    if (rv == 0)
        return 1;
    return 0;
}

/* path is assumed to be MAXPATHLEN */
static char *compute_checkpoint_filename(journal_t *j, const char *subscriber, char *name)
{
    size_t len = strlen(j->path);
    memcpy(name, j->path, len);
    name[len++] = IFS_CH;
    name[len++] = 'c';
    name[len++] = 'p';
    name[len++] = '.';
    for (const char *sub = subscriber; *sub; ) {
        name[len++] = journal_hexchars[((*sub & 0xf0) >> 4)];
        name[len++] = journal_hexchars[(*sub & 0x0f)];
        sub++;
    }
    name[len] = '\0';

    DEBUG("checkpoint '%s' filename is '%s'.", subscriber, name);

    return name;
}

static journal_file_t *journal_open_named_checkpoint(journal_t *j, const char *cpname, int flags)
{
    char name[MAXPATHLEN];
    compute_checkpoint_filename(j, cpname, name);
    return journal_file_open(j, name, flags, j->file_mode);
}

static int journal_munmap_reader(journal_ctx_t *ctx)
{
    if (ctx->mmap_base) {
        munmap(ctx->mmap_base, ctx->mmap_len);
        ctx->mmap_base = NULL;
        ctx->mmap_len = 0;
    }
    return 0;
}

static int journal_teardown_reader(journal_ctx_t *ctx)
{
    ctx->reader_is_initialized = 0;
    ctx->data_file_size = 0;
    return journal_munmap_reader(ctx);
}

static int journal_close_reader(journal_ctx_t *ctx)
{
    journal_teardown_reader(ctx);
    if (ctx->data) {
        journal_file_close(ctx->journal, ctx->data);
        ctx->data = NULL;
    }
    return 0;
}

static int journal_close_indexer(journal_ctx_t *ctx)
{
    if (ctx->index) {
        journal_file_close(ctx->journal, ctx->index);
        ctx->index = NULL;
    }

    return 0;
}

static journal_file_t *journal_open_reader(journal_ctx_t *ctx, uint32_t log)
{
    char file[MAXPATHLEN];

    memset(file, 0, sizeof(file));
    if (ctx->current_log != log) {
        journal_close_reader(ctx);
        journal_close_indexer(ctx);
    }

    if (ctx->data)
        return ctx->data;

    journal_set_data_file(ctx, file, log);

    DEBUG("opening log file[ro]: '%s'.", file);

    ctx->data = journal_file_open(ctx->journal, file, 0, ctx->file_mode);
    ctx->current_log = log;

    return ctx->data;
}

static int journal_mmap_reader(journal_ctx_t *ctx, uint32_t log)
{
    if (ctx->current_log == log && ctx->mmap_base)
        return 0;

    journal_open_reader(ctx, log);
    if (!ctx->data)
        return -1;

    if (!journal_file_map_read(ctx->data, &ctx->mmap_base, &ctx->mmap_len)) {
        ctx->mmap_base = NULL;
        ctx->last_error = JOURNAL_ERR_FILE_READ;
        ctx->last_errno = errno;
        return -1;
    }

    ctx->data_file_size = ctx->mmap_len;
    return 0;
}

static int journal_setup_reader(journal_ctx_t *ctx, uint32_t log, u_int8_t force_mmap)
{
    journal_read_method_type_t read_method = ctx->read_method;

    switch (read_method) {
    case JOURNAL_READ_METHOD_MMAP: {
        int rv = journal_mmap_reader(ctx, log);
        if (rv != 0)
            return -1;
        ctx->reader_is_initialized = 1;
    }   return 0;
    case JOURNAL_READ_METHOD_PREAD:
        if (force_mmap) {
            int rv = journal_mmap_reader(ctx, log);
            if (rv != 0)
                return -1;
        } else {
            off_t file_size = journal_file_size(ctx->data);
            if (file_size < 0)
                SYS_FAIL(JOURNAL_ERR_FILE_SEEK);
            ctx->data_file_size = file_size;
        }
        ctx->reader_is_initialized = 1;
        return 0;
    default:
        SYS_FAIL(JOURNAL_ERR_NOT_SUPPORTED);
        break;
    }
finish:
    return -1;
}

static int journal_unlink_datafile(journal_ctx_t *ctx, uint32_t log)
{
    char file[MAXPATHLEN];
    int len;

    memset(file, 0, sizeof(file));
    if (ctx->current_log == log) {
        journal_close_reader(ctx);
        journal_close_indexer(ctx);
    }

    journal_set_data_file(ctx, file, log);
    DEBUG("unlinking '%s'.", file);
    unlink(file);

    len = strlen(file);
    if ((len + sizeof(INDEX_EXT)) > sizeof(file))
        return -1;
    memcpy(file + len, INDEX_EXT, sizeof(INDEX_EXT));
    DEBUG("unlinking '%s'.", file);
    unlink(file);
    return 0;
}

int journal_pending_readers(journal_t *j, uint32_t log, uint32_t *earliest_out)
{
    int readers = 0;

    DIR *dir = opendir(j->path);
    if (dir == NULL)
        return -1;

    char file[MAXPATHLEN] = {0};
    int len = strlen(j->path);
    if (len + 2 > (int)sizeof(file)) {
        closedir(dir);
        return -1;
    }
    memcpy(file, j->path, len);
    file[len++] = IFS_CH;
    file[len] = '\0';

    int seen = 0;
    uint32_t earliest = 0;
    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (ent->d_name[0] == 'c' && ent->d_name[1] == 'p' && ent->d_name[2] == '.') {
            int dlen = strlen(ent->d_name);
            if ((len + dlen + 1) > (int)sizeof(file))
                continue;
            memcpy(file + len, ent->d_name, dlen + 1); /* include \0 */
            DEBUG("Checking if '%s' needs '%s'...", ent->d_name, j->path);
            journal_file_t *cp = journal_file_open(j, file, 0, j->file_mode);
            if (cp != NULL) {
                if (journal_file_lock(cp)) {
                    journal_id_t id;
                    (void) journal_file_pread(cp, &id, sizeof(id), 0);
                    DEBUG("\t%u <= %u (pending reader).", id.log, log);
                    if (!seen) {
                        earliest = id.log;
                        seen = 1;
                    } else {
                        if (id.log < earliest)
                            earliest = id.log;
                    }
                    if (id.log <= log)
                        readers++;
                    journal_file_unlock(cp);
                }
                journal_file_close(j, cp);
            }
        }
    }

    closedir(dir);

    if (earliest_out)
        *earliest_out = earliest;

    return readers;
}

int journal_ctx_get_checkpoint(journal_ctx_t *ctx, const char *s, journal_id_t *id)
{
    journal_file_t *f;
    int rv = -1;

    if (ctx->subscriber_name && (s == NULL || !strcmp(ctx->subscriber_name, s))) {
        if (!ctx->checkpoint)
            ctx->checkpoint = journal_open_named_checkpoint(ctx->journal, ctx->subscriber_name, 0);
        f = ctx->checkpoint;
    } else {
        f = journal_open_named_checkpoint(ctx->journal, s, 0);
    }

    if (f) {
        if (journal_file_lock(f)) {
            if (journal_file_pread(f, id, sizeof(*id), 0))
                rv = 0;
            if (journal_file_pread(f, id, sizeof(*id), 0))
            journal_file_unlock(f);
        }
    }

    if (f && f != ctx->checkpoint)
        journal_file_close(ctx->journal, f);

    return rv;
}

static int journal_set_checkpoint(journal_ctx_t *ctx, const char *s, const journal_id_t *id)
{
    journal_file_t *f;
    int rv = -1;
    journal_id_t old_id;
    uint32_t log;

    if (ctx->subscriber_name && !strcmp(ctx->subscriber_name, s)) {
        if (!ctx->checkpoint)
            ctx->checkpoint = journal_open_named_checkpoint(ctx->journal, s, 0);
        f = ctx->checkpoint;
    } else {
        f = journal_open_named_checkpoint(ctx->journal, s, 0);
    }

    if (f == NULL)
        return -1;
    if (!journal_file_lock(f))
        goto failset;

    if (journal_file_size(f) == 0) {
        /* we're setting it for the first time, no segments were pending on it */
        old_id.log = id->log;
    } else {
        if (!journal_file_pread(f, &old_id, sizeof(old_id), 0))
            goto failset;
    }

    if (!journal_file_pwrite(f, id, sizeof(*id), 0)) {
        ERROR("journal_file_pwrite failed in journal_set_checkpoint");
        ctx->last_error = JOURNAL_ERR_FILE_WRITE;
        goto failset;
    }

    if (ctx->meta->safety == JOURNAL_SAFE)
        journal_file_sync(f);

    journal_file_unlock(f);
    rv = 0;

    for (log = old_id.log; log < id->log; log++) {
        if (journal_pending_readers(ctx->journal, log, NULL) == 0)
            journal_unlink_datafile(ctx, log);
    }

 failset:
    if (f && f != ctx->checkpoint)
        journal_file_close(ctx->journal, f);

    return rv;
}

static int journal_close_metastore(journal_ctx_t *ctx)
{
    if (ctx->metastore) {
        journal_file_close(ctx->journal, ctx->metastore);
        ctx->metastore = NULL;
    }

    if (ctx->meta_is_mapped) {
        munmap((void *)ctx->meta, sizeof(*ctx->meta));
        ctx->meta = &ctx->pre_init;
        ctx->meta_is_mapped = 0;
    }
    return 0;
}

static char *compute_metastore_filename(journal_t *j, char *file, size_t file_size)
{
    memset(file, 0, file_size);
    DEBUG("journal_open_metastore");
    int len = strlen(j->path);
    if ((len + 1 /* IFS_CH */ + 9 /* "metastore" */ + 1) > MAXPATHLEN) {
        ERROR("compute_metastore_filename: filename too long");
        return NULL;
    }
    memcpy(file, j->path, len);
    file[len++] = IFS_CH;
    memcpy(&file[len], "metastore", 10); /* "metastore" + '\0' */

    return file;
}

static int journal_open_metastore(journal_ctx_t *ctx, bool create)
{
    char file[MAXPATHLEN];

    if (compute_metastore_filename(ctx->journal, file,  sizeof(file)) == NULL) {
        ctx->last_error = JOURNAL_ERR_CREATE_META;
        return -1;
    }

    ctx->metastore = journal_file_open(ctx->journal, file, create ? O_CREAT : O_RDWR, ctx->file_mode);

    if (!ctx->metastore) {
        ctx->last_errno = errno;
        ERROR("journal_open_metastore: file create failed");
        ctx->last_error = JOURNAL_ERR_CREATE_META;
        return -1;
    }

    return 0;
}

static int journal_save_metastore(journal_ctx_t *ctx, bool ilocked)
{
    DEBUG("journal_save_metastore.");

    if (ctx->context_mode == JOURNAL_READ) {
        ERROR("journal_save_metastore: illegal call in JOURNAL_READ mode");
        ctx->last_error = JOURNAL_ERR_ILLEGAL_WRITE;
        ctx->last_errno = EPERM;
        return -1;
    }

    if (!ilocked && !journal_file_lock(ctx->metastore)) {
        ERROR("journal_save_metastore: cannot get lock");
        ctx->last_error = JOURNAL_ERR_LOCK;
        return -1;
    }

    if (ctx->meta_is_mapped) {
        int rv, flags = MS_INVALIDATE;
        if (ctx->meta->safety == JOURNAL_SAFE)
            flags |= MS_SYNC;

        rv = msync((void *)(ctx->meta), sizeof(*ctx->meta), flags);
        if (!ilocked)
            journal_file_unlock(ctx->metastore);
        if (rv < 0) {
            ERROR("journal_save_metastore msync failed");
            ctx->last_error = JOURNAL_ERR_FILE_WRITE;
        }
        return rv;
    } else {
        if (!journal_file_pwrite(ctx->metastore, ctx->meta, sizeof(*ctx->meta), 0)) {
            if (!ilocked) journal_file_unlock(ctx->metastore);
            ERROR("journal_file_pwrite failed");
            ctx->last_error = JOURNAL_ERR_FILE_WRITE;
            return -1;
        }
        if (ctx->meta->safety == JOURNAL_SAFE) {
            journal_file_sync(ctx->metastore);
        }
    }

    if (!ilocked)
        journal_file_unlock(ctx->metastore);

    return 0;
}

static int journal_init_metastore(journal_t *j)
{
    char file[MAXPATHLEN];

    if (compute_metastore_filename(j, file,  sizeof(file)) == NULL)
        return -1;

    journal_file_t *metastore = journal_file_open(j, file, O_CREAT, j->file_mode);

    if (metastore == NULL) {
        ERROR("journal_init_metastore: file create failed");
        return -1;
    }

    if (!journal_file_pwrite(metastore, &j->meta, sizeof(j->meta), 0)) {
        ERROR("journal_file_pwrite failed");
        journal_file_close(j, metastore);
        return -1;
    }

    journal_file_sync(metastore);

    journal_file_close(j, metastore);

    return 0;
}

static int ___journal_resync_index(journal_ctx_t *ctx, uint32_t log, journal_id_t *last,
                                                       int *closed)
{
    uint64_t indices[BUFFERED_INDICES];
    journal_message_header_compressed_t logmhdr;
    uint32_t *message_disk_len = &logmhdr.mlen;
    off_t index_off, data_off, data_len, recheck_data_len;
    size_t hdr_size = sizeof(journal_message_header_t);
    uint64_t index;
    int i, second_try = 0;

    if (IS_COMPRESS_MAGIC(ctx)) {
        hdr_size = sizeof(journal_message_header_compressed_t);
        message_disk_len = &logmhdr.compressed_len;
    }

    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (closed)
        *closed = 0;

    journal_open_reader(ctx, log);
    if (!ctx->data) {
        ctx->last_error = JOURNAL_ERR_FILE_OPEN;
        ctx->last_errno = errno;
        return -1;
    }

#define RESTART do { \
    if (second_try == 0) { \
        journal_file_truncate(ctx->index, index_off); \
        journal_file_unlock(ctx->index); \
        second_try = 1; \
        ctx->last_error = JOURNAL_ERR_SUCCESS; \
        goto restart; \
    } \
    SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT); \
} while (0)

restart:
    journal_open_indexer(ctx, log);
    if (!ctx->index) {
        ctx->last_error = JOURNAL_ERR_IDX_OPEN;
        ctx->last_errno = errno;
        return -1;
    }

    if (!journal_file_lock(ctx->index)) {
        ctx->last_error = JOURNAL_ERR_LOCK;
        ctx->last_errno = errno;
        return -1;
    }

    data_off = 0;
    if ((data_len = journal_file_size(ctx->data)) == -1)
        SYS_FAIL(JOURNAL_ERR_FILE_SEEK);

    if (data_len == 0 && log < ctx->meta->storage_log) {
        journal_unlink_datafile(ctx, log);
        ctx->last_error = JOURNAL_ERR_FILE_OPEN;
        ctx->last_errno = ENOENT;
        return -1;
    }

    if ((index_off = journal_file_size(ctx->index)) == -1)
        SYS_FAIL(JOURNAL_ERR_IDX_SEEK);

    if (index_off % sizeof(uint64_t)) {
        DEBUG("corrupt index [%ld].", index_off);
        RESTART;
    }

    if (index_off > (off_t)sizeof(uint64_t)) {
        if (!journal_file_pread(ctx->index, &index, sizeof(index), index_off - sizeof(uint64_t)))
            SYS_FAIL(JOURNAL_ERR_IDX_READ);

        if (index == 0) {
            /* This log file has been "closed" */
            DEBUG("index closed.");
            if (last) {
                last->log = log;
                last->marker = (index_off / sizeof(uint64_t)) - 1;
            }
            if (closed) *closed = 1;
            goto finish;
        } else {
            if (index > (uint64_t)data_len) {
                DEBUG("index told me to seek somehwere I can't.");
                RESTART;
            }
            data_off = index;
        }
    }

    if (index_off > 0) {
        /* We are adding onto a partial index so we must advance a record */
        if (!journal_file_pread(ctx->data, &logmhdr, hdr_size, data_off))
            SYS_FAIL(JOURNAL_ERR_FILE_READ);
        if ((data_off += hdr_size + *message_disk_len) > data_len) {
            DEBUG("index overshoots %lu + %zu + %"PRIu32" > %zd.",
                  data_off - hdr_size - *message_disk_len, hdr_size, *message_disk_len, data_len);
            RESTART;
        }
    }

    i = 0;
    while (data_off + hdr_size <= (size_t)data_len) {
        off_t next_off = data_off;
        if (!journal_file_pread(ctx->data, &logmhdr, hdr_size, data_off))
            SYS_FAIL(JOURNAL_ERR_FILE_READ);
        if (logmhdr.reserved != ctx->meta->hdr_magic) {
            DEBUG("logmhdr.reserved == %"PRIu32".", logmhdr.reserved);
            SYS_FAIL(JOURNAL_ERR_FILE_CORRUPT);
        }
        if ((next_off += hdr_size + *message_disk_len) > data_len)
            break;

        /* Write our new index offset */
        indices[i++] = data_off;
        if (i >= BUFFERED_INDICES) {
            DEBUG("writing %i offsets.", i);
            if (!journal_file_pwrite(ctx->index, indices, i * sizeof(uint64_t), index_off))
                RESTART;
            index_off += i * sizeof(uint64_t);
            i = 0;
        }
        data_off = next_off;
    }

    if (i > 0) {
        DEBUG("writing %i offsets.", i);
        if (!journal_file_pwrite(ctx->index, indices, i * sizeof(uint64_t), index_off))
            RESTART;
        index_off += i * sizeof(uint64_t);
    }

    if (last) {
        last->log = log;
        last->marker = index_off / sizeof(uint64_t);
    }

    if (log < ctx->meta->storage_log) {

        /*
         * the writer may have moved on and incremented the storage_log
         * while we were building this index.  This doesn't mean
         * that this index file is complete because the writer
         * probably wrote out data to the data file before incrementing
         * the storage_log.
         *
         * We need to recheck the data file length
         * and ensure it's not larger than when we checked it earlier
         * only if the data_len has not changed can we consider this index closed.
         *
         * If the data file length has changed, simply RESTART and rebuild this index file
         */
        if ((recheck_data_len = journal_file_size(ctx->data)) == -1)
            SYS_FAIL(JOURNAL_ERR_FILE_SEEK);

        if (recheck_data_len != data_len) {
            DEBUG("data len changed, %ld -> %ld.", data_off, recheck_data_len);
            RESTART;
        }

        if (data_off != data_len) {
            DEBUG("closing index, but %ld != %ld.", data_off, data_len);
            SYS_FAIL(JOURNAL_ERR_FILE_CORRUPT);
        }

        /* Special case: if we are closing, we next write a '0'
         * we can't write the closing marker if the data segment had no records
         * in it, since it will be confused with an index to offset 0 by the
         * next reader; this only happens when segments are repaired */
        if (index_off) {
            index = 0;
            if (!journal_file_pwrite(ctx->index, &index, sizeof(uint64_t), index_off)) {
                DEBUG("null index.");
                RESTART;
            }
            index_off += sizeof(uint64_t);
        }

        if (closed)
            *closed = 1;
    }
#undef RESTART

finish:
    journal_file_unlock(ctx->index);
    DEBUG("index is %s.", closed?(*closed?"closed":"open"):"unknown");
    if (ctx->last_error == JOURNAL_ERR_SUCCESS)
        return 0;
    return -1;
}

static int journal_repair_datafile(journal_ctx_t *ctx, uint32_t log)
{
    journal_message_header_compressed_t hdr;
    size_t hdr_size = sizeof(journal_message_header_t);
    uint32_t *message_disk_len = &hdr.mlen;

    if (IS_COMPRESS_MAGIC(ctx)) {
        hdr_size = sizeof(journal_message_header_compressed_t);
        message_disk_len = &hdr.compressed_len;
    }

    char *this, *next, *afternext = NULL, *mmap_end;
    int i, invalid_count = 0;
    struct {
        off_t start, end;
    } *invalid = NULL;
    off_t orig_len, src, dst, len;

#define TAG_INVALID(s, e) do { \
    if (invalid_count) \
        invalid = realloc(invalid, (invalid_count + 1) * sizeof(*invalid)); \
    else \
        invalid = malloc(sizeof(*invalid)); \
    invalid[invalid_count].start = s - (char *)ctx->mmap_base; \
    invalid[invalid_count].end = e - (char *)ctx->mmap_base; \
    invalid_count++; \
} while (0)

    ctx->last_error = JOURNAL_ERR_SUCCESS;

    /* we want the reader's open logic because this runs in the read path
     * the underlying fds are always RDWR anyway */
    journal_open_reader(ctx, log);
    if (!ctx->data) {
        ctx->last_error = JOURNAL_ERR_FILE_OPEN;
        ctx->last_errno = errno;
        return -1;
    }
    if (!journal_file_lock(ctx->data)) {
        ctx->last_error = JOURNAL_ERR_LOCK;
        ctx->last_errno = errno;
        return -1;
    }
    if (journal_setup_reader(ctx, log, 1) != 0)
        SYS_FAIL(ctx->last_error);

    orig_len = ctx->mmap_len;
    mmap_end = (char*)ctx->mmap_base + ctx->mmap_len;
    /* these values will cause us to fall right into the error clause and
     * start searching for a valid header from offset 0 */
    this = (char*)ctx->mmap_base - hdr_size;
    hdr.reserved = ctx->meta->hdr_magic;
    hdr.mlen = 0;

    while (this + hdr_size <= mmap_end) {
        next = this + hdr_size + *message_disk_len;
        if (next <= (char *)ctx->mmap_base)
            goto error;
        if (next == mmap_end) {
            this = next;
            break;
        }
        if (next + hdr_size > mmap_end)
            goto error;
        memcpy(&hdr, next, hdr_size);
        if (hdr.reserved != ctx->meta->hdr_magic)
            goto error;
        this = next;
        continue;
    error:
        for (next = this + hdr_size; next + hdr_size <= mmap_end; next++) {
            memcpy(&hdr, next, hdr_size);
            if (hdr.reserved == ctx->meta->hdr_magic) {
                afternext = next + hdr_size + *message_disk_len;
                if (afternext <= (char *)ctx->mmap_base)
                    continue;
                if (afternext == mmap_end)
                    break;
                if (afternext + hdr_size > mmap_end)
                    continue;
                memcpy(&hdr, afternext, hdr_size);
                if (hdr.reserved == ctx->meta->hdr_magic)
                    break;
            }
        }

        /* correct for while loop entry condition */
        if (this < (char *)ctx->mmap_base)
            this = ctx->mmap_base;
        if (next + hdr_size > mmap_end)
            break;
        if (next > this)
            TAG_INVALID(this, next);
        this = afternext;
    }

    if (this != mmap_end)
        TAG_INVALID(this, mmap_end);

#undef TAG_INVALID

#define MOVE_SEGMENT do { \
    char cpbuff[4096]; \
    off_t chunk; \
    while (len > 0) { \
        chunk = len; \
        if (chunk > (off_t)sizeof(cpbuff)) chunk = sizeof(cpbuff); \
        if (!journal_file_pread(ctx->data, &cpbuff, chunk, src)) \
            SYS_FAIL(JOURNAL_ERR_FILE_READ); \
        if (!journal_file_pwrite(ctx->data, &cpbuff, chunk, dst)) \
            SYS_FAIL(JOURNAL_ERR_FILE_WRITE); \
        src += chunk; \
        dst += chunk; \
        len -= chunk; \
    } \
} while (0)

    if (invalid_count > 0) {
        journal_teardown_reader(ctx);
        dst = invalid[0].start;
        for (i = 0; i < invalid_count - 1; ) {
            src = invalid[i].end;
            len = invalid[++i].start - src;
            MOVE_SEGMENT;
        }
        src = invalid[invalid_count - 1].end;
        len = orig_len - src;
        if (len > 0)
            MOVE_SEGMENT;
        if (!journal_file_truncate(ctx->data, dst))
            SYS_FAIL(JOURNAL_ERR_FILE_WRITE);
    }

#undef MOVE_SEGMENT

finish:
    journal_file_unlock(ctx->data);
    if (invalid)
        free(invalid);
    if (ctx->last_error != JOURNAL_ERR_SUCCESS)
        return -1;
    return invalid_count;
}

static int journal_resync_index(journal_ctx_t *ctx, uint32_t log, journal_id_t *last, int *closed)
{
    int attempts, rv = -1;

    for (attempts=0; attempts<4; attempts++) {
        rv = ___journal_resync_index(ctx, log, last, closed);
        if (ctx->last_error == JOURNAL_ERR_SUCCESS)
            break;
        if ((ctx->last_error == JOURNAL_ERR_FILE_OPEN) || (ctx->last_error == JOURNAL_ERR_IDX_OPEN))
            break;

        /* We can't fix the file if someone may write to it again */
        if (log >= ctx->meta->storage_log)
            break;

        journal_file_lock(ctx->index);
        /* it doesn't really matter what journal_repair_datafile returns
         * we'll keep retrying anyway */
        journal_repair_datafile(ctx, log);
        journal_file_truncate(ctx->index, 0);
        journal_file_unlock(ctx->index);
    }

    return rv;
}


static int findel(DIR *dir, unsigned int *earp, unsigned int *latp)
{
    unsigned int maxx = 0;
    unsigned int minn = 0;
    unsigned int hexx = 0;
    struct dirent *ent;
    int havemaxx = 0;
    int haveminn = 0;
    int nent = 0;

    if (dir == NULL)
        return 0;
    (void)rewinddir(dir);
    while ( (ent = readdir(dir)) != NULL ) {
        if (ent->d_name[0] != '\0') {
            nent++;
            if ((strlen(ent->d_name) == 8) && (sscanf(ent->d_name, "%x", &hexx) == 1)) {
                if (havemaxx == 0) {
                    havemaxx = 1;
                    maxx = hexx;
                } else {
                    if (hexx > maxx)
                        maxx = hexx;
                }
                if (haveminn == 0) {
                    haveminn = 1;
                    minn = hexx;
                } else {
                    if (hexx < minn)
                        minn = hexx;
                }
            }
        }
    }
    if ((havemaxx == 1) && (latp != NULL))
        *latp = maxx;
    if ((haveminn == 1) && (earp != NULL))
        *earp = minn;
    // a valid directory has at least . and .. entries
    return (nent >= 2);
}

static int journal_get_storage_bounds(journal_ctx_t *ctx, unsigned int *earliest, unsigned *latest)
{
    DIR *dir = opendir(ctx->path);
    if (dir == NULL) {
        ERROR("cannot open journal directory '%s'",ctx->path);
        ctx->last_error = JOURNAL_ERR_NOTDIR;
        return 0;
    }
    int b0 = findel(dir, earliest, latest);
    (void)closedir(dir);
    return b0;
}

static int new_checkpoint(char *ag, int fd, journal_id_t point)
{
    int newfd = 0;
    int sta = 0;

    if ((ag == NULL) || (ag[0] == '\0')) {
        ERROR("invalid checkpoint path");
        return 0;
    }

    if (fd < 0) {
        (void)unlink(ag);
        fd = creat(ag, DEFAULT_FILE_MODE);
        if (fd < 0) {
            ERROR("cannot create checkpoint file");
            return 0;
        } else {
            newfd = 1;
        }
    }

    int x = ftruncate(fd, 0);
    if (x >= 0) {
        off_t xcvR = lseek(fd, 0, SEEK_SET);
        if (xcvR == 0) {
            unsigned int goal[2];
            goal[0] = point.log;
            goal[1] = point.marker;
            int wr = write(fd, goal, sizeof(goal));
            if (wr != sizeof(goal))
                ERROR("cannot write checkpoint file");
            sta = (wr == sizeof(goal));
        } else {
            ERROR("cannot seek to beginning of checkpoint file");
        }
    } else {
        ERROR("ftruncate failed to zero out checkpoint file");
    }

    if (newfd == 1)
        (void)close(fd);

    return sta;
}

static int validate_metastore(const journal_meta_info_t *info, journal_meta_info_t *out)
{
    int valid = 1;
    if ((info->hdr_magic == DEFAULT_HDR_MAGIC) || (IS_COMPRESS_MAGIC_HDR(info->hdr_magic))) {
        if (out)
            out->hdr_magic = info->hdr_magic;
    } else {
        valid = 0;
    }

    if (info->unit_limit > 0) {
        if (out)
            out->unit_limit = info->unit_limit;
    } else {
        valid = 0;
    }

    if ((info->safety == JOURNAL_UNSAFE) ||
        (info->safety == JOURNAL_ALMOST_SAFE) ||
        (info->safety == JOURNAL_SAFE)) {
        if (out)
            out->safety = info->safety;
    } else {
        valid = 0;
    }

    return valid;
}

static int metastore_ok_p(char *ag, unsigned int lat, journal_meta_info_t *out)
{
    journal_meta_info_t current;
    if (out) {
        /* setup the real defaults */
        out->storage_log = lat;
        out->unit_limit = 4*1024*1024;
        out->safety = 1;
        out->hdr_magic = DEFAULT_HDR_MAGIC;
    }
    int fd = open(ag, O_RDONLY);
    if (fd < 0)
        return 0;

    if (lseek(fd, 0, SEEK_END) != sizeof(current)) {
        (void)close(fd);
        return 0;
    }

    (void)lseek(fd, 0, SEEK_SET);
    int rd = read(fd, &current, sizeof(current));
    (void)close(fd);
    fd = -1;
    if (rd != sizeof(current))
        return 0;

    /* validate */
    int valid = validate_metastore(&current, out);
    if (current.storage_log != lat) {
        // we don't need to set out->storage_log, as it was set at the outset of this function
        valid = 0;
    }
    return valid;
}

static int repair_metastore(journal_t *j, const char *pth, unsigned int lat)
{
    if (pth == NULL)
        pth = j->path;

    if ((pth == NULL) || (pth[0] == '\0')) {
        ERROR("invalid metastore path");
        return 0;
    }

    size_t leen = strlen(pth);
    if ((leen == 0) || (leen > (MAXPATHLEN-12))) {
        ERROR("invalid metastore path length");
        return 0;
    }

    size_t leen2 = leen + strlen("metastore") + 4;
    char *ag = (char *)calloc(leen2, sizeof(char));
    if (ag == NULL)  /* out of memory, so bail */
        return 0;

    (void)ssnprintf(ag, leen2-1, "%s%cmetastore", pth, IFS_CH);

    journal_meta_info_t out;
    int b = metastore_ok_p(ag, lat, &out);
    if (b != 0) {
        ERROR("metastore integrity check failed");
        return 1;
    }
    int fd = open(ag, O_RDWR|O_CREAT, DEFAULT_FILE_MODE);
    free(ag);
    ag = NULL;
    if (fd < 0) {
        ERROR("cannot create new metastore file");
        return 0;
    }
    if (ftruncate(fd, sizeof(out)) != 0)
        ERROR("ftruncate failed (non-fatal)");
    int wr = write(fd, &out, sizeof(out));
    (void)close(fd);
    if (wr != sizeof(out))
        ERROR("cannot write new metastore file");
    return (wr == sizeof(out));
}

static int repair_checkpointfile(journal_ctx_t *ctx, const char *pth, unsigned int ear,
                                                     unsigned int lat)
{
    DIR *dir = opendir(pth);
    if (dir == NULL) {
        ERROR("invalid directory: '%s'", pth);
        return 0;
    }
    struct dirent *ent = NULL;
    int fd = -1;

    const size_t twoI = 2*sizeof(unsigned int);
    int rv = 0;
    while ((ent = readdir(dir)) != NULL) {
        int sta = 0;
        /* cp.7e -> cp.~.... */
        if (strncmp(ent->d_name, "cp.", 3) != 0)
            continue;
        if (strncmp(ent->d_name, "cp.7e", 5) == 0)
            continue;
        size_t leen = strlen(pth) + strlen(ent->d_name) + 5;
        if (leen >= MAXPATHLEN) {
            ERROR("invalid checkpoint path length");
            continue;
        }
        char *ag = (char *)calloc(leen+1, sizeof(char));
        if (ag == NULL)
            continue;
        (void)ssnprintf(ag, leen-1, "%s%c%s", pth, IFS_CH, ent->d_name);
        int closed;
        journal_id_t last;
        fd = open(ag, O_RDWR);
        sta = 0;
        if (fd >= 0) {
            off_t oof = lseek(fd, 0, SEEK_END);
            (void)lseek(fd, 0, SEEK_SET);
            if (oof == (off_t)twoI) {
                unsigned int have[2];
                int rd = read(fd, have, sizeof(have));
                if (rd == sizeof(have)) {
                    journal_id_t expect, current;
                    expect.log = current.log = have[0];
                    expect.marker = current.marker = have[1];
                    if (expect.log < ear) {
                        ERROR("checkpoint %s log too small %08x -> %08x",
                              ent->d_name, expect.log, ear);
                        expect.log = ear;
                    }
                    if (expect.log > lat) {
                        ERROR("checkpoint %s log too big %08x -> %08x",
                              ent->d_name, expect.log, ear);
                        expect.log = lat;
                    }
                    if (journal_resync_index(ctx, expect.log, &last, &closed)) {
                        ERROR("could not resync index for %08x", have[0]);
                        last.log = expect.log;
                        last.marker = 0;
                    }
                    if ((last.log != current.log) || (last.marker < current.marker)) {
                        ERROR("fixing checkpoint %s data %08x:%08x != %08x:%08x", ent->d_name,
                              current.log, current.marker, last.log, last.marker);
                    } else {
                        sta = 1;
                        rv++;
                    }
                } else {
                    ERROR("cannot read checkpoint file %s", ent->d_name);
                }
            } else {
                ERROR("checkpoint %s file size incorrect: %ld != %zu", ent->d_name, oof, twoI);
            }
        } else {
            ERROR("cannot open checkpoint file %s", ent->d_name);
        }

        if (sta == 0) {
            sta = new_checkpoint(ag, fd, last);
            if (sta == 0)
                ERROR("cannot create new checkpoint file");
        }
        if (fd >= 0) {
            (void)close(fd);
            fd = -1;
        }
        if (ag != NULL) {
            (void)free((void *)ag);
            ag = NULL;
        }
    }
    closedir(dir);
    return rv;
}

static int journal_inspect_datafile(journal_ctx_t *ctx, uint32_t log)
{
    journal_message_header_compressed_t hdr;
    size_t hdr_size = sizeof(journal_message_header_t);
    uint32_t *message_disk_len = &hdr.mlen;
    char *this, *next, *mmap_end;
    int i;
    time_t timet;
    struct tm tm;
    char tbuff[128];

    if (IS_COMPRESS_MAGIC(ctx)) {
        hdr_size = sizeof(journal_message_header_compressed_t);
        message_disk_len = &hdr.compressed_len;
    }

    ctx->last_error = JOURNAL_ERR_SUCCESS;

    journal_open_reader(ctx, log);
    if (!ctx->data)
        SYS_FAIL(JOURNAL_ERR_FILE_OPEN);
    if (journal_setup_reader(ctx, log, 1) != 0)
        SYS_FAIL(ctx->last_error);

    mmap_end = (char*)ctx->mmap_base + ctx->mmap_len;
    this = ctx->mmap_base;
    i = 0;
    while (this + hdr_size <= mmap_end) {
        memcpy(&hdr, this, hdr_size);
        i++;
        if (hdr.reserved != ctx->meta->hdr_magic) {
            ERROR("Message %d at [%ld] has invalid reserved value %u.",
                  i, (long int)(this - (char *)ctx->mmap_base), hdr.reserved);
            return 1;
        }

        next = this + hdr_size + *message_disk_len;
        if (next <= (char *)ctx->mmap_base) {
            ERROR("Message %d at [%ld] of (%lu+%u) wrapped to negative offset!", i, \
                  (long int)(this - (char *)ctx->mmap_base), \
                  (long unsigned int)hdr_size, *message_disk_len); \
            return 1;
        }
        if (next > mmap_end) {
            ERROR("Message %d at [%ld] of (%lu+%u) off the end!", i, \
                  (long int)(this - (char *)ctx->mmap_base), \
                  (long unsigned int)hdr_size, *message_disk_len); \
            return 1;
        }

        timet = hdr.tv_sec;
        localtime_r(&timet, &tm);
        strftime(tbuff, sizeof(tbuff), "%Y-%m-%dT%H:%M:%S", &tm);
        this = next;
    }

    if (this < mmap_end) {
        ERROR("%ld bytes of junk at the end", (long int)(mmap_end - this));
        return 1;
    }

    return 0;
finish:
    return -1;
}

static int analyze_datafile(journal_ctx_t *ctx, uint32_t logid)
{
    char idxfile[MAXPATHLEN];
    int rv = 0;

    if (journal_inspect_datafile(ctx, logid) > 0) {
        WARNING("Repairing %s/%08x.", ctx->path, logid);
        rv = journal_repair_datafile(ctx, logid);
        journal_set_data_file(ctx, idxfile, logid);
        strcat(idxfile, INDEX_EXT);
        unlink(idxfile);
    }

    return rv;
}

static int repair_data_files(journal_ctx_t *log)
{
    if (log->context_mode == JOURNAL_READ) {
        ERROR("repair_data_files: illegal call in JOURNAL_READ mode");
        log->last_error = JOURNAL_ERR_ILLEGAL_WRITE;
        log->last_errno = EPERM;
        return -1;
    }

    DIR *dir = opendir(log->path);
    if (!dir) {
        log->last_error = JOURNAL_ERR_NOTDIR;
        log->last_errno = errno;
        return -1;
    }

    int rv = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        uint32_t logid;
        if (is_datafile(de->d_name, &logid)) {
            char fullfile[MAXPATHLEN];
            char fullidx[MAXPATHLEN];
            struct stat st;
            int readers;
            ssnprintf(fullfile, sizeof(fullfile), "%s/%s", log->path, de->d_name);
            ssnprintf(fullidx, sizeof(fullidx), "%s/%s" INDEX_EXT, log->path, de->d_name);
            if (stat(fullfile, &st) == 0) {
                readers = journal_pending_readers(log->journal, logid, NULL);
                if (analyze_datafile(log, logid) < 0)
                    rv = -1;
                if (readers == 0) {
                    unlink(fullfile);
                    unlink(fullidx);
                }
            }
        }
    }
    closedir(dir);
    return rv;
}

int journal_ctx_repair(journal_ctx_t *ctx, int aggressive)
{
    // step 1: extract the directory path
    const char *pth;

    if (ctx != NULL)
        pth = ctx->path;
    else
        pth = NULL; // fassertxgetpath();

    if (pth == NULL || pth[0] == '\0') {
        ERROR("repair command cannot find journal path");
        ctx->last_error = JOURNAL_ERR_NOTDIR;
        return 0;                               /* hopeless without a dir name */
    }

    // step 2: find the earliest and the latest files with hex names
    unsigned int ear = 0;
    unsigned int lat = 0;
    int b0 = journal_get_storage_bounds(ctx, &ear, &lat);
    if (b0 == 1) {
        // step 3: attempt to repair the metastore. It might not need any
        // repair, in which case nothing will happen
        int b1 = repair_metastore(ctx->journal, pth, lat);
        // step 4: attempt to repair the checkpoint file. It might not need
        // any repair, in which case nothing will happen
        int b2 = repair_checkpointfile(ctx, pth, ear, lat);

        // if aggressive repair is not authorized, fail
        if (aggressive != 0) {
            if (repair_data_files(ctx) < 0) {
                return 1;
            }
        }

        if (b1 != 1) {
            ctx->last_error = JOURNAL_ERR_CREATE_META;
            return 0;
        }

        if (b2 != 1) {
            ctx->last_error = JOURNAL_ERR_CHECKPOINT;
            return 1;
        }
    } else if (b0 == 0) {
        ERROR("cannot find hex files in journal directory");
    }

    ctx->last_error = JOURNAL_ERR_SUCCESS;
    return 0;
}

static int journal_restore_metastore(journal_ctx_t *ctx, int ilocked, int readonly)
{
    void *base = NULL;
    size_t len = 0;
    if (ctx->meta_is_mapped)
        return 0;
    DEBUG("journal_restore_metastore");

    if (!ilocked && !journal_file_lock(ctx->metastore)) {
        ERROR("journal_restore_metastore: cannot get lock");
        ctx->last_error = JOURNAL_ERR_LOCK;
        return -1;
    }

    if (ctx->meta_is_mapped == 0) {
        int rv;

        if (readonly == 1) {
            rv = journal_file_map_read(ctx->metastore, &base, &len);
        } else {
            rv = journal_file_map_rdwr(ctx->metastore, &base, &len);
        }

        if (rv != 1) {
            ERROR("journal_file_map_%s failed", readonly == 1 ? "read" : "rdwr");
            if (!ilocked)
                journal_file_unlock(ctx->metastore);
            ctx->last_error = JOURNAL_ERR_OPEN;
            return -1;
        }

         journal_meta_info_t *meta = base;
         if (len != sizeof(*meta) || !validate_metastore(meta, NULL)) {
             if (!repair_metastore(ctx->journal, NULL, meta->storage_log)) {
                 if (!ilocked)
                     journal_file_unlock(ctx->metastore);
                 ctx->last_error = JOURNAL_ERR_OPEN;
                 return -1;
             }
         }

        if ((rv != 1) || (len != sizeof(*ctx->meta))) {
            ERROR("journal_file_map_%s failed", readonly == 1 ? "read" : "rdwr");
            if (!ilocked)
                journal_file_unlock(ctx->metastore);
            ctx->last_error = JOURNAL_ERR_OPEN;
            return -1;
        }

        ctx->meta = base;
        ctx->meta_is_mapped = 1;
    }

    if (!ilocked)
        journal_file_unlock(ctx->metastore);

    if (ctx->meta != &ctx->pre_init)
        ctx->pre_init.hdr_magic = ctx->meta->hdr_magic;

    return 0;
}

static int journal_metastore_atomic_increment(journal_ctx_t *ctx)
{
    char file[MAXPATHLEN] = {0};

    DEBUG("atomic increment on %u.", ctx->current_log);

    if (ctx->data)
        SYS_FAIL(JOURNAL_ERR_NOT_SUPPORTED);

    if (!journal_file_lock(ctx->metastore))
        SYS_FAIL(JOURNAL_ERR_LOCK);

    if (journal_restore_metastore(ctx, 1, 0)) {
        ERROR("journal_metastore_atomic_increment call to journal_restore_metastore failed");
        SYS_FAIL(JOURNAL_ERR_META_OPEN);
    }

    if (ctx->meta->storage_log == ctx->current_log) {
        /* We're the first ones to it, so we get to increment it */
        ctx->current_log++;
        journal_set_data_file(ctx, file, ctx->current_log);
        ctx->data = journal_file_open(ctx->journal, file, O_CREAT, ctx->file_mode);
        ctx->meta->storage_log = ctx->current_log;
        if (journal_save_metastore(ctx, true)) {
            ERROR("journal_metastore_atomic_increment call to journal_save_metastore failed");
            SYS_FAIL(JOURNAL_ERR_META_OPEN);
        }
    }
finish:
    journal_file_unlock(ctx->metastore);
    /* Now we update our curent_log to the current storage_log,
     * it may have advanced farther than we know.
     */
    ctx->current_log = ctx->meta->storage_log;
    if (ctx->last_error == JOURNAL_ERR_SUCCESS) return 0;
    return -1;
}

static int journal_close_writer(journal_ctx_t *ctx)
{
    if (ctx->data) {
        journal_file_sync(ctx->data);
        journal_file_close(ctx->journal, ctx->data);
        ctx->data = NULL;
    }
    return 0;
}

static journal_file_t *journal_open_writer(journal_ctx_t *ctx)
{
    char file[MAXPATHLEN] = {0};

    if (ctx->data) {
        /* Still open */
        return ctx->data;
    }

    if (!journal_file_lock(ctx->metastore))
        SYS_FAIL(JOURNAL_ERR_LOCK);

    int x = journal_restore_metastore(ctx, 1, 0);
    if (x) {
        ERROR("journal_open_writer call to journal_restore_metastore failed");
        SYS_FAIL(JOURNAL_ERR_META_OPEN);
    }

    ctx->current_log = ctx->meta->storage_log;
    journal_set_data_file(ctx, file, ctx->current_log);
    DEBUG("opening log file[rw]: '%s'.", file);
    ctx->data = journal_file_open(ctx->journal, file, O_CREAT, ctx->file_mode);
    if (ctx->data == NULL) {
        ERROR("journal_open_writer call to journal_file_open failed");
        ctx->last_error = JOURNAL_ERR_FILE_OPEN;
    } else {
        ctx->last_error = JOURNAL_ERR_SUCCESS;
    }

finish:
    journal_file_unlock(ctx->metastore);
    return ctx->data;
}

int journal_ctx_write_message(journal_ctx_t *ctx, journal_message_t *mess, struct timeval *when)
{
    struct timeval now;
    journal_message_header_compressed_t hdr;
    off_t current_offset = 0;
    size_t hdr_size = sizeof(journal_message_header_t);

    if (IS_COMPRESS_MAGIC(ctx))
        hdr_size = sizeof(journal_message_header_compressed_t);

    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_APPEND) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_WRITE;
        ctx->last_errno = EPERM;
        return -1;
    }

    /* build the data we want to write outside of any lock */
    hdr.reserved = ctx->meta->hdr_magic;
    if (when) {
        hdr.tv_sec = when->tv_sec;
        hdr.tv_usec = when->tv_usec;
    } else {
        gettimeofday(&now, NULL);
        hdr.tv_sec = now.tv_sec;
        hdr.tv_usec = now.tv_usec;
    }

    /* we store the original message size in the header */
    hdr.mlen = mess->mess_len;

    struct iovec v[2];
    v[0].iov_base = (void *) &hdr;
    v[0].iov_len = hdr_size;

    /* create a stack space to compress into which is large enough for most messages to compress into */
    char compress_space[16384] = {0};
    v[1].iov_base = compress_space;
    size_t compressed_len = sizeof(compress_space);

    if (IS_COMPRESS_MAGIC(ctx)) {
        if (journal_compress(mess->mess, mess->mess_len,
                             (char **)&v[1].iov_base, &compressed_len) != 0) {
            ERROR("journal_compress failed in journal_ctx_write_message");
            SYS_FAIL(JOURNAL_ERR_FILE_WRITE);
        }
        hdr.compressed_len = compressed_len;
        v[1].iov_len = hdr.compressed_len;
    } else {
        v[1].iov_base = mess->mess;
        v[1].iov_len = mess->mess_len;
    }

    size_t total_size = v[0].iov_len + v[1].iov_len;

    /* now grab the file lock and write to file */
    /**
     * this needs to be synchronized as concurrent writers can
     * overwrite the shared ctx->data pointer as they move through
     * individual file segments.
     *
     * Thread A-> open, write to existing segment,
     * Thread B-> check open (already open)
     * Thread A-> close and null out ctx->data pointer
     * Thread B-> wha?!?
     */
    pthread_mutex_lock(&ctx->write_lock);

    journal_open_writer(ctx);
    if (!ctx->data) {
        ctx->last_error = JOURNAL_ERR_FILE_OPEN;
        ctx->last_errno = errno;
        pthread_mutex_unlock(&ctx->write_lock);
        return -1;
    }

    if (!journal_file_lock(ctx->data)) {
        ctx->last_error = JOURNAL_ERR_LOCK;
        ctx->last_errno = errno;
        pthread_mutex_unlock(&ctx->write_lock);
        return -1;
    }

    if ((current_offset = journal_file_size(ctx->data)) == -1)
        SYS_FAIL(JOURNAL_ERR_FILE_SEEK);

    if (!journal_file_pwritev_verify_return_value(ctx->data, v, 2, current_offset, total_size)) {
        ERROR("journal_file_pwritev failed in journal_ctx_write_message");
        SYS_FAIL(JOURNAL_ERR_FILE_WRITE);
    }
    current_offset += total_size;;

    if (IS_COMPRESS_MAGIC(ctx) && v[1].iov_base != compress_space)
        free(v[1].iov_base);

    if (ctx->meta->unit_limit <= current_offset) {
        journal_file_unlock(ctx->data);
        journal_close_writer(ctx);
        journal_metastore_atomic_increment(ctx);
        pthread_mutex_unlock(&ctx->write_lock);
        return 0;
    }

finish:
    journal_file_unlock(ctx->data);
    pthread_mutex_unlock(&ctx->write_lock);
    if (ctx->last_error == JOURNAL_ERR_SUCCESS) return 0;
    return -1;
}

int journal_ctx_write(journal_ctx_t *ctx, void *data, size_t len)
{
    journal_message_t msg;
    msg.mess = (void *)data;
    msg.mess_len = len;
    return journal_ctx_write_message(ctx, &msg, NULL);
}

int journal_ctx_read_checkpoint(journal_ctx_t *ctx, const journal_id_t *chkpt)
{
    ctx->last_error = JOURNAL_ERR_SUCCESS;

    if (ctx->context_mode != JOURNAL_READ) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_CHECKPOINT;
        ctx->last_errno = EPERM;
        return -1;
    }

    if (journal_set_checkpoint(ctx, ctx->subscriber_name, chkpt) != 0) {
        ctx->last_error = JOURNAL_ERR_CHECKPOINT;
        ctx->last_errno = 0;
        return -1;
    }

    return 0;
}

int journal_remove_subscriber(journal_t *j, const char *s)
{
    char name[MAXPATHLEN];

    compute_checkpoint_filename(j, s, name);
    int rv = unlink(name);

    if (rv == 0)
        return 1;

    if (errno == ENOENT)
        return 0;

    return -1;
}

journal_ctx_t *journal_get_writer(journal_t *j)
{
    int rv;
    struct stat sb;

    journal_ctx_t *ctx = journal_ctx_new(j);
    if (ctx == NULL)
        return NULL;

    pthread_mutex_lock(&ctx->write_lock);
#if 0
    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_NEW) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_OPEN;
        pthread_mutex_unlock(&ctx->write_lock);
        return -1;
    }
#endif

    ctx->context_mode = JOURNAL_APPEND;
    while ((rv = stat(ctx->path, &sb)) == -1 && errno == EINTR);
    if (rv == -1)
        SYS_FAIL(JOURNAL_ERR_OPEN);
    if (!S_ISDIR(sb.st_mode))
        SYS_FAIL(JOURNAL_ERR_NOTDIR);

    if (journal_open_metastore(ctx, false) != 0) {
        ERROR("journal_get_writer call to journal_open_metastore failed");
        SYS_FAIL(JOURNAL_ERR_META_OPEN);
    }

    if (journal_restore_metastore(ctx, 0, 0)) {
        ERROR("journal_get_writer call to journal_restore_metastore failed");
        SYS_FAIL(JOURNAL_ERR_META_OPEN);
    }

finish:
    pthread_mutex_unlock(&ctx->write_lock);
    if (ctx->last_error == JOURNAL_ERR_SUCCESS)
        return ctx;
    ctx->context_mode = JOURNAL_INVALID;
    journal_ctx_close(ctx);
    return NULL;
}

journal_ctx_t *journal_get_reader(journal_t *j, const char *subscriber)
{
    int rv;
    struct stat sb;
    journal_id_t dummy;

    journal_ctx_t *ctx = journal_ctx_new(j);
    if (ctx == NULL)
        return NULL;

#if 0
    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_NEW) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_OPEN;
        return -1;
    }
#endif

    ctx->context_mode = JOURNAL_READ;
    ctx->subscriber_name = strdup(subscriber);

    while ((rv = stat(ctx->path, &sb)) == -1 && errno == EINTR);
    if (rv == -1)
        SYS_FAIL(JOURNAL_ERR_OPEN);

    if (!S_ISDIR(sb.st_mode))
        SYS_FAIL(JOURNAL_ERR_NOTDIR);

    if (journal_open_metastore(ctx, false) != 0) {
        ERROR("journal_ctx_open_reader call to journal_open_metastore failed");
        SYS_FAIL(JOURNAL_ERR_META_OPEN);
    }

    if (journal_ctx_get_checkpoint(ctx, ctx->subscriber_name, &dummy))
        SYS_FAIL(JOURNAL_ERR_INVALID_SUBSCRIBER);

    if (journal_restore_metastore(ctx, 0, 1)) {
        ERROR("journal_ctx_open_reader call to journal_restore_metastore failed");
        SYS_FAIL(JOURNAL_ERR_META_OPEN);
    }

finish:
    if (ctx->last_error == JOURNAL_ERR_SUCCESS)
        return ctx;

    //ctx->context_mode = JOURNAL_INVALID;
    journal_ctx_close(ctx);
    return NULL;
}

static int journal_close_checkpoint(journal_ctx_t *ctx)
{
    if (ctx->checkpoint) {
        journal_file_close(ctx->journal, ctx->checkpoint);
        ctx->checkpoint = NULL;
    }
    return 0;
}

int journal_config_checksum(journal_t *j, bool checksum)
{
    j->checksum = checksum;

    return 0;
}

int journal_config_compress(journal_t *j, bool compress)
{
    j->compress = compress;

    return 0;
}

int journal_config_retention_size(journal_t *j, off_t retention_size)
{
    j->retention_size = retention_size;

    return 0;
}

int journal_config_retention_time(journal_t *j, cdtime_t retention_time)
{
    j->retention_time = retention_time;
    return 0;
}

int journal_config_segment_size(journal_t *j, off_t segment_size)
{
    j->segment_size = segment_size;

    return 0;
}

int journal_init(journal_t *j)
{
    int rv;
    struct stat sb;

    j->last_error = JOURNAL_ERR_SUCCESS;
    if (strlen(j->path) > MAXLOGPATHLEN-1) {
        j->last_error = JOURNAL_ERR_CREATE_PATHLEN;
        return -1;
    }

#if 0
    if (ctx->context_mode != JOURNAL_NEW) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_INIT;
        return -1;
    }
#endif

#if 0
    ctx->context_mode = JOURNAL_INIT;
#endif

    while ((rv = stat(j->path, &sb)) == -1 && errno == EINTR);
    if (rv == 0 || errno != ENOENT) {
        j->last_error = JOURNAL_ERR_CREATE_EXISTS;
        return -1;
    }

    int dirmode = j->file_mode;
    if (dirmode & 0400)
        dirmode |= 0100;
    if (dirmode & 040)
        dirmode |= 010;
    if (dirmode & 04)
        dirmode |= 01;

    if (mkdir(j->path, dirmode) == -1) {
        j->last_error = JOURNAL_ERR_CREATE_MKDIR;
        return -1;
    }

    chmod(j->path, dirmode);
    // fassertxsetpath(ctx->path);
    /* Setup our initial state and store our instance metadata */
    if (journal_init_metastore(j) != 0) {
        ERROR("journal_init failed to init metastore");
        return -1;
    }

    return 0;
}

int journal_ctx_close(journal_ctx_t *ctx)
{
    journal_close_writer(ctx);
    journal_close_indexer(ctx);
    journal_close_reader(ctx);
    journal_close_metastore(ctx);
    journal_close_checkpoint(ctx);
    free(ctx->subscriber_name);
    free(ctx->path);
    free(ctx->compressed_data_buffer);
    free(ctx->mess_data);
    free(ctx);
    return 0;
}

static journal_ctx_t *journal_ctx_new(journal_t *j)
{
    journal_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return NULL;

    ctx->meta = &ctx->pre_init;
    ctx->pre_init.unit_limit = DEFAULT_UNIT_LIMIT;
    ctx->pre_init.safety = DEFAULT_SAFETY;
    ctx->pre_init.hdr_magic = DEFAULT_HDR_MAGIC;
    ctx->file_mode = DEFAULT_FILE_MODE;
    ctx->read_method = DEFAULT_READ_MESSAGE_TYPE;
    ctx->context_mode = JOURNAL_NEW;
    ctx->path = strdup(j->path);
    pthread_mutex_init(&ctx->write_lock, NULL);
    //  fassertxsetpath(path);
    ctx->journal = j;

    return ctx;
}

static journal_file_t *journal_open_indexer(journal_ctx_t *ctx, uint32_t log)
{
    char file[MAXPATHLEN];
    int len;

    memset(file, 0, sizeof(file));
    if (ctx->current_log != log) {
        journal_close_reader(ctx);
        journal_close_indexer(ctx);
    }

    if (ctx->index) {
        return ctx->index;
    }

    journal_set_data_file(ctx, file, log);

    len = strlen(file);
    if ((len + sizeof(INDEX_EXT)) > sizeof(file))
        return NULL;

    memcpy(file + len, INDEX_EXT, sizeof(INDEX_EXT));

    DEBUG("opening index file: '%s'.", file);

    ctx->index = journal_file_open(ctx->journal, file, O_CREAT, ctx->file_mode);
    ctx->current_log = log;

    return ctx->index;
}

static int journal_find_first_log_after(journal_ctx_t *ctx, journal_id_t *chkpt,
                                        journal_id_t *start, journal_id_t *finish)
{
    journal_id_t last;
    int closed;

    memcpy(start, chkpt, sizeof(*chkpt));
 attempt:
    if (journal_resync_index(ctx, start->log, &last, &closed) != 0) {
        if (ctx->last_error == JOURNAL_ERR_FILE_OPEN && ctx->last_errno == ENOENT) {
            char file[MAXPATHLEN];
            int ferr;
            struct stat sb = {0};

            memset(file, 0, sizeof(file));
            journal_set_data_file(ctx, file, start->log + 1);
            while ((ferr = stat(file, &sb)) == -1 && errno == EINTR);
            /* That file doesn't exist... bad, but we can fake a recovery by
                 advancing the next file that does exist */
            ctx->last_error = JOURNAL_ERR_SUCCESS;
            if (start->log >= ctx->meta->storage_log || (ferr != 0 && errno != ENOENT)) {
                /* We don't advance past where people are writing */
                memcpy(finish, start, sizeof(*start));
                return 0;
            }
            start->marker = 0;
            start->log++;  /* BE SMARTER! */
            goto attempt;
        }
        return -1; /* Just persist resync's error state */
    }

    /* If someone checkpoints off the end, be nice */
    if (last.log == start->log && last.marker < start->marker)
        memcpy(start, &last, sizeof(*start));

    if (!memcmp(start, &last, sizeof(last)) && closed) {
        if (start->log >= ctx->meta->storage_log) {
            /* We don't advance past where people are writing */
            memcpy(finish, start, sizeof(*start));
            return 0;
        }
        start->marker = 0;
        start->log++;
        goto attempt;
    }

    memcpy(finish, &last, sizeof(last));
    return 0;
}

int journal_ctx_read_interval(journal_ctx_t *ctx, journal_id_t *start, journal_id_t *finish)
{
    journal_id_t chkpt;
    int count = 0;

    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_READ) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_WRITE;
        ctx->last_errno = EPERM;
        return -1;
    }

    journal_restore_metastore(ctx, 0, 1);
    if (journal_ctx_get_checkpoint(ctx, ctx->subscriber_name, &chkpt))
        SYS_FAIL(JOURNAL_ERR_INVALID_SUBSCRIBER);
    if (journal_find_first_log_after(ctx, &chkpt, start, finish) != 0)
        goto finish; /* Leave whatever error was set in find_first_log_after */

    if (start->log != chkpt.log)
        start->marker = 0;
    else
        start->marker = chkpt.marker;

    if (start->log != chkpt.log) {
        /* We've advanced our checkpoint, let's not do this work again */
        if (journal_set_checkpoint(ctx, ctx->subscriber_name, start) != 0)
            SYS_FAIL(JOURNAL_ERR_CHECKPOINT);
    }
    /* Here 'start' is actually the checkpoint, so we must advance it one.
         However, that may not be possible, if there are no messages, so first
         make sure finish is bigger */
    count = finish->marker - start->marker;
    if (finish->marker > start->marker)
        start->marker++;

    /* If the count is less than zero, the checkpoint is off the end
     * of the file. When this happens, we need to set it to the end of
     * the file */
    if (count < 0) {
        WARNING("need to repair checkpoint for %s - start (%08x:%08x) > finish (%08x:%08x).",
                ctx->path, start->log, start->marker, finish->log, finish->marker);
        if (journal_set_checkpoint(ctx, ctx->subscriber_name, finish) != 0) {
            WARNING("failed repairing checkpoint for %s.", ctx->path);
            SYS_FAIL(JOURNAL_ERR_CHECKPOINT);
        }

        if (journal_ctx_get_checkpoint(ctx, ctx->subscriber_name, &chkpt)) {
            /* Should never happen */
            SYS_FAIL(JOURNAL_ERR_INVALID_SUBSCRIBER);
        }

        WARNING("repaired checkpoint for %s: %08x:%08x.", ctx->path, chkpt.log, chkpt.marker);
        ctx->last_error = JOURNAL_ERR_SUCCESS;
        count = 0;
    }

    /* We need to munmap it, so that we can remap it with more data if needed */
    journal_teardown_reader(ctx);

finish:
    if (ctx->last_error == JOURNAL_ERR_SUCCESS)
        return count;

    return -1;
}

int journal_ctx_read_message(journal_ctx_t *ctx, const journal_id_t *id, journal_message_t *m)
{
    off_t index_len;
    uint64_t data_off;
    int with_lock = 0;
    size_t hdr_size = 0;
    uint32_t *message_disk_len = &m->aligned_header.mlen;
    /* We don't want the style to change mid-read, so use whatever
     * the style is now */
    journal_read_method_type_t read_method = ctx->read_method;

    if (IS_COMPRESS_MAGIC(ctx)) {
        hdr_size = sizeof(journal_message_header_compressed_t);
        message_disk_len = &m->aligned_header.compressed_len;
    } else {
        hdr_size = sizeof(journal_message_header_t);
    }

 once_more_with_lock:

    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_READ)
        SYS_FAIL(JOURNAL_ERR_ILLEGAL_WRITE);
    if (id->marker < 1)
        SYS_FAIL(JOURNAL_ERR_ILLEGAL_LOGID);

    journal_open_reader(ctx, id->log);
    if (!ctx->data)
        SYS_FAIL(JOURNAL_ERR_FILE_OPEN);
    journal_open_indexer(ctx, id->log);
    if (!ctx->index)
        SYS_FAIL(JOURNAL_ERR_IDX_OPEN);

    if (with_lock) {
        if (!journal_file_lock(ctx->index)) {
            with_lock = 0;
            SYS_FAIL(JOURNAL_ERR_LOCK);
        }
    }

    if ((index_len = journal_file_size(ctx->index)) == -1)
        SYS_FAIL(JOURNAL_ERR_IDX_SEEK);
    if (index_len % sizeof(uint64_t))
        SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT);
    if (id->marker * (off_t)sizeof(uint64_t) > index_len)
        SYS_FAIL(JOURNAL_ERR_ILLEGAL_LOGID);

    if (!journal_file_pread(ctx->index, &data_off, sizeof(uint64_t),
                                        (id->marker - 1) * sizeof(uint64_t))) {
        SYS_FAIL(JOURNAL_ERR_IDX_READ);
    }
    if (data_off == 0 && id->marker != 1) {
        if (id->marker * (off_t)sizeof(uint64_t) == index_len) {
            /* close tag; not a real offset */
            ctx->last_error = JOURNAL_ERR_CLOSE_LOGID;
            ctx->last_errno = 0;
            if (with_lock) journal_file_unlock(ctx->index);
            return -1;
        } else {
            /* an offset of 0 in the middle of an index means corruption */
            SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT);
        }
    }

    if (journal_setup_reader(ctx, id->log, 0) != 0)
        SYS_FAIL(ctx->last_error);

    switch(read_method) {
    case JOURNAL_READ_METHOD_MMAP:
        if (data_off > ctx->mmap_len - hdr_size) {
            DEBUG("read idx off end: %"PRIu64".", data_off);
            SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT);
        }

        memcpy(&m->aligned_header, ((u_int8_t *)ctx->mmap_base) + data_off,
                     hdr_size);

        if (data_off + hdr_size + *message_disk_len > ctx->mmap_len) {
            DEBUG("read idx off end: %"PRIu64" %zu.", data_off, ctx->mmap_len);
            SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT);
        }
        m->header = &m->aligned_header;
        if (IS_COMPRESS_MAGIC(ctx)) {
            if (ctx->mess_data_size < m->aligned_header.mlen) {
                ctx->mess_data = realloc(ctx->mess_data, m->aligned_header.mlen * 2);
                ctx->mess_data_size = m->aligned_header.mlen * 2;
            }
            journal_decompress((((char *)ctx->mmap_base) + data_off + hdr_size),
                               m->header->compressed_len, ctx->mess_data, ctx->mess_data_size);
            m->mess_len = m->header->mlen;
            m->mess = ctx->mess_data;
        } else {
            m->mess_len = m->header->mlen;
            m->mess = (((u_int8_t *)ctx->mmap_base) + data_off + hdr_size);
        }
        break;
    case JOURNAL_READ_METHOD_PREAD:
        if (data_off > ctx->data_file_size - hdr_size) {
            DEBUG("read idx off end: %"PRIu64".", data_off);
            SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT);
        }
        if (!journal_file_pread(ctx->data, &m->aligned_header, hdr_size, data_off))
            SYS_FAIL(JOURNAL_ERR_IDX_READ);

        if (data_off + hdr_size + *message_disk_len > ctx->data_file_size) {
            DEBUG("read idx off end: %"PRIu64" %lu.", data_off, ctx->data_file_size);
            SYS_FAIL(JOURNAL_ERR_IDX_CORRUPT);
        }

        m->header = &m->aligned_header;
        if (ctx->mess_data_size < m->aligned_header.mlen) {
            ctx->mess_data_size = m->aligned_header.mlen * 2;
            ctx->mess_data = realloc(ctx->mess_data, ctx->mess_data_size);
        }

        if (IS_COMPRESS_MAGIC(ctx)) {
            if (ctx->compressed_data_buffer_len < m->aligned_header.compressed_len) {
                ctx->compressed_data_buffer_len = m->aligned_header.compressed_len * 2;
                ctx->compressed_data_buffer = realloc(ctx->compressed_data_buffer,
                                                      ctx->compressed_data_buffer_len);
            }
            if (!journal_file_pread(ctx->data, ctx->compressed_data_buffer,
                                    m->aligned_header.compressed_len, data_off + hdr_size)) {
                SYS_FAIL(JOURNAL_ERR_IDX_READ);
            }
            journal_decompress((char *)ctx->compressed_data_buffer,
                               m->header->compressed_len, ctx->mess_data, ctx->mess_data_size);
        } else {
            if (!journal_file_pread(ctx->data, ctx->mess_data, m->aligned_header.mlen,
                                               data_off + hdr_size))
                SYS_FAIL(JOURNAL_ERR_IDX_READ);
        }
        m->mess_len = m->header->mlen;
        m->mess = ctx->mess_data;
        break;
    default:
        break;
    }

finish:
    if (with_lock)
        journal_file_unlock(ctx->index);
    if (ctx->last_error == JOURNAL_ERR_SUCCESS)
        return 0;

    if (!with_lock) {
        if (ctx->last_error == JOURNAL_ERR_IDX_CORRUPT) {
            if (journal_file_lock(ctx->index)) {
                journal_file_truncate(ctx->index, 0);
                journal_file_unlock(ctx->index);
            }
        }
        ___journal_resync_index(ctx, id->log, NULL, NULL);
        with_lock = 1;
        DEBUG("read retrying with lock.");
        goto once_more_with_lock;
    }

    return -1;
}

int journal_ctx_first_log_id(journal_ctx_t *ctx, journal_id_t *id)
{
    struct dirent *de;
    ctx->last_error = JOURNAL_ERR_SUCCESS;
    uint32_t log;
    int found = 0;

    id->log = 0xffffffff;
    id->marker = 0;
    DIR *d = opendir(ctx->path);
    if (d == NULL)
        return -1;

    while ((de = readdir(d))) {
        char *cp = de->d_name;
        if (strlen(cp) != 8)
            continue;
        log = 0;
        int i;
        for (i=0;i<8;i++) {
            log <<= 4;
            if (cp[i] >= '0' && cp[i] <= '9')
                log |= (cp[i] - '0');
            else if (cp[i] >= 'a' && cp[i] <= 'f')
                log |= (cp[i] - 'a' + 0xa);
            else if (cp[i] >= 'A' && cp[i] <= 'F')
                log |= (cp[i] - 'A' + 0xa);
            else
                break;
        }
        if (i != 8)
            continue;
        found = 1;
        if (log < id->log)
            id->log = log;
    }
    if (!found)
        id->log = 0;
    closedir(d);
    return 0;
}

int journal_ctx_last_storage_log(journal_ctx_t *ctx, uint32_t *logid)
{
    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_READ) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_WRITE;
        ctx->last_errno = EPERM;
        return -1;
    }

    if (journal_restore_metastore(ctx, 0, 1) != 0)
        return -1;

    *logid = ctx->meta->storage_log;

    return 0;
}

int journal_ctx_last_log_id(journal_ctx_t *ctx, journal_id_t *id)
{
    ctx->last_error = JOURNAL_ERR_SUCCESS;
    if (ctx->context_mode != JOURNAL_READ) {
        ctx->last_error = JOURNAL_ERR_ILLEGAL_WRITE;
        ctx->last_errno = EPERM;
        return -1;
    }

    if (journal_restore_metastore(ctx, 0, 1) != 0)
        return -1;

    journal_resync_index(ctx, ctx->meta->storage_log, id, NULL);
    if (ctx->last_error == JOURNAL_ERR_SUCCESS)
        return 0;

    return -1;
}

int journal_ctx_advance_id(journal_ctx_t *ctx, journal_id_t *cur, journal_id_t *start,
                           journal_id_t *finish)
{
    int rv;

    if (memcmp(cur, finish, sizeof(journal_id_t))) {
        start->marker++;
    } else {
        if ((rv = journal_find_first_log_after(ctx, cur, start, finish)) != 0)
            return rv;
        if (cur->log != start->log)
            start->marker = 1;
        else start->marker = cur->marker;
    }

    return 0;
}


int journal_ctx_add_subscriber(journal_ctx_t *ctx, const char *s, journal_position_t whence)
{
    journal_id_t chkpt;
    journal_ctx_t *tmpctx = NULL;
    ctx->last_error = JOURNAL_ERR_SUCCESS;

    journal_file_t *jchkpt = journal_open_named_checkpoint(ctx->journal, s, O_CREAT|O_EXCL);
    if (!jchkpt) {
        ctx->last_errno = errno;
        if (errno == EEXIST)
            ctx->last_error = JOURNAL_ERR_SUBSCRIBER_EXISTS;
        else
            ctx->last_error = JOURNAL_ERR_OPEN;
        return -1;
    }

    journal_file_close(ctx->journal, jchkpt);

    if (whence == JOURNAL_BEGIN) {
        memset(&chkpt, 0, sizeof(chkpt));
        journal_ctx_first_log_id(ctx, &chkpt);
        if (journal_set_checkpoint(ctx, s, &chkpt) != 0) {
            ctx->last_error = JOURNAL_ERR_CHECKPOINT;
            ctx->last_errno = 0;
            return -1;
        }
        return 0;
    }

    if (whence == JOURNAL_END) {
        journal_id_t start, finish;
        memset(&chkpt, 0, sizeof(chkpt));
        if (journal_open_metastore(ctx, false) != 0) {
            ERROR("journal_ctx_add_subscriber call to journal_open_metastore failed");
            SYS_FAIL(JOURNAL_ERR_META_OPEN);
        }
        if (journal_restore_metastore(ctx, 0, 1) != 0) {
            ERROR("journal_ctx_add_subscriber call to journal_restore_metastore failed");
            SYS_FAIL(JOURNAL_ERR_META_OPEN);
        }
        chkpt.log = ctx->meta->storage_log;
        if (journal_set_checkpoint(ctx, s, &chkpt) != 0)
            SYS_FAIL(JOURNAL_ERR_CHECKPOINT);
        tmpctx = journal_get_reader(ctx->journal, s);
        if (tmpctx == NULL)
            goto finish;
        if (journal_ctx_read_interval(tmpctx, &start, &finish) < 0)
            goto finish;
        journal_ctx_close(tmpctx);
        tmpctx = NULL;
        if (journal_set_checkpoint(ctx, s, &finish) != 0)
            SYS_FAIL(JOURNAL_ERR_CHECKPOINT);
        return 0;
    }

    ctx->last_error = JOURNAL_ERR_NOT_SUPPORTED;

finish:
    if (tmpctx)
        journal_ctx_close(tmpctx);

    return -1;
}

#if 0
int journal_ctx_add_subscriber_copy_checkpoint(journal_ctx_t *old_ctx, const char *new,
                                               const char *old)
{
    journal_id_t chkpt;
    /* If there's no old checkpoint available, just return */
    if (journal_ctx_get_checkpoint(old_ctx, old, &chkpt))
        return -1;

    /* If we can't open the journal_ctx, just return */
    journal_ctx_t *new_ctx = journal_new(old_ctx->path);
    if (!new_ctx)
        return -1;

    if (journal_ctx_add_subscriber(new_ctx, new, JOURNAL_BEGIN)) {
        /* If it already exists, we want to overwrite it */
        if (errno != EEXIST) {
            journal_ctx_close(new_ctx);
            return -1;
        }
    }

    /* Open a reader for the new subscriber */
    if (journal_ctx_open_reader(new_ctx, new) < 0) {
        journal_ctx_close(new_ctx);
        return -1;
    }

    /* Set the checkpoint of the new subscriber to
     * the old subscriber's checkpoint */
    if (journal_ctx_read_checkpoint(new_ctx, &chkpt))
        return -1;

    journal_ctx_close(new_ctx);
    return 0;
}
#endif

int journal_ctx_set_subscriber_checkpoint(journal_ctx_t *ctx, const char *s,
                                          const journal_id_t *checkpoint)
{
    if (journal_ctx_add_subscriber(ctx, s, JOURNAL_BEGIN)) {
        if (errno != EEXIST)
            return -1;
    }

    return journal_set_checkpoint(ctx, s, checkpoint);
}

int journal_ctx_list_subscribers_dispose(__attribute__((unused)) journal_ctx_t *ctx, char **subs)
{
    if (subs) {
        int i = 0;
        char *s;
        while ((s = subs[i++]) != NULL)
            free(s);
        free(subs);
    }

    return 0;
}

int journal_ctx_list_subscribers(journal_ctx_t *ctx, char ***subs)
{
    struct _journal_subs js = { NULL, 0, 0 };
    struct dirent *ent;
    unsigned char file[MAXPATHLEN];
    char *p;
    int len;

    memset(file, 0, sizeof(file));
    js.subs = calloc(16, sizeof(char *));
    js.allocd = 16;

    DIR *dir = opendir(ctx->path);
    if (!dir)
        return -1;
    while ((ent = readdir(dir))) {
        if (ent->d_name[0] == 'c' && ent->d_name[1] == 'p' && ent->d_name[2] == '.') {

            for (len = 0, p = ent->d_name + 3; *p;) {
                unsigned char c;
                int i;

                for (c = 0, i = 0; i < 16; i++) {
                    if (journal_hexchars[i] == *p) {
                        c = i << 4;
                        break;
                    }
                }
                p++;
                for (i = 0; i < 16; i++) {
                    if (journal_hexchars[i] == *p) {
                        c |= i;
                        break;
                    }
                }
                p++;
                file[len++] = c;
            }
            file[len] = '\0';

            js.subs[js.used++] = strdup((char *)file);
            if (js.used == js.allocd) {
                js.allocd *= 2;
                js.subs = realloc(js.subs, js.allocd*sizeof(char *));
            }
            js.subs[js.used] = NULL;
        }
    }
    closedir(dir);
    *subs = js.subs;
    return js.used;
}

size_t journal_ctx_raw_size(journal_ctx_t *ctx)
{
    DIR *dir = opendir(ctx->path);
    if (dir == NULL)
        return 0;

    int len = strlen(ctx->path);
    char filename[MAXPATHLEN] = {0};
    memcpy(filename, ctx->path, len);
    filename[len++] = IFS_CH;

    size_t totalsize = 0;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        int dlen = strlen(de->d_name);
        if ((len + dlen + 1) > (int)sizeof(filename))
            continue;
        memcpy(filename+len, de->d_name, dlen + 1); /* include \0 */
        struct stat sb;
        int ferr;
        while ((ferr = stat(filename, &sb)) == -1 && errno == EINTR);
        if (ferr == 0 && S_ISREG(sb.st_mode))
            totalsize += sb.st_size;
    }

    closedir(dir);

    return totalsize;
}

#if 0
int journal_clean(const char *file)
{
    int rv = 0;

    journal_ctx_t *log = journal_new(file);
    journal_ctx_open_writer(log);

    DIR *dir = opendir(file);
    if (!dir)
        goto out;

    uint32_t earliest = 0;
    if (journal_pending_readers(log->journal, 0, &earliest) < 0)
        goto out;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        uint32_t logid;
        if (is_datafile(de->d_name, &logid) && logid < earliest) {
            char fullfile[MAXPATHLEN];
            char fullidx[MAXPATHLEN];

            memset(fullfile, 0, sizeof(fullfile));
            memset(fullidx, 0, sizeof(fullidx));
            ssnprintf(fullfile, sizeof(fullfile), "%s/%s", file, de->d_name);
            ssnprintf(fullidx, sizeof(fullidx), "%s/%s" INDEX_EXT, file, de->d_name);
            (void)unlink(fullfile);
            (void)unlink(fullidx); /* this may not exist; don't care */
            rv++;
        }
    }
    closedir(dir);
 out:
    journal_ctx_close(log);
    return rv;
}
#endif

int journal_ctx_alter_journal_size(journal_ctx_t *ctx, size_t size)
{
    if (ctx->meta->unit_limit == size)
        return 0;

    if (ctx->context_mode == JOURNAL_APPEND || ctx->context_mode == JOURNAL_NEW) {
        ctx->meta->unit_limit = size;
        if (ctx->context_mode == JOURNAL_APPEND) {
            if (journal_save_metastore(ctx, false) != 0) {
                ERROR("journal_ctx_alter_journal_size call to journal_save_metastore failed");
                SYS_FAIL(JOURNAL_ERR_CREATE_META);
            }
        }
        return 0;
    }

finish:
    return -1;
}

int journal_ctx_alter_mode(journal_ctx_t *ctx, int mode)
{
    ctx->file_mode = mode;
    return 0;
}

int journal_ctx_alter_read_method(journal_ctx_t *ctx, journal_read_method_type_t method)
{
    /* Cannot change read method mid-processing */
    if (ctx->reader_is_initialized)
        return -1;

    ctx->read_method = method;
    return 0;
}

int journal_ctx_alter_safety(journal_ctx_t *ctx, journal_safety_t safety)
{
    if (ctx->meta->safety == safety)
        return 0;

    if (ctx->context_mode == JOURNAL_APPEND || ctx->context_mode == JOURNAL_NEW) {
        ctx->meta->safety = safety;
        if (ctx->context_mode == JOURNAL_APPEND) {
            if (journal_save_metastore(ctx, false) != 0) {
                ERROR("journal_ctx_alter_safety call to journal_save_metastore failed");
                SYS_FAIL(JOURNAL_ERR_CREATE_META);
            }
        }
        return 0;
    }

finish:
    return -1;
}

int journal_snprint_logid(char *b, int n, const journal_id_t *id)
{
    return ssnprintf(b, n, "%08x:%08x", id->log, id->marker);
}

int journal_ctx_set_use_compression(journal_ctx_t *ctx, bool use)
{
    if (use) {
        ctx->pre_init.hdr_magic = DEFAULT_HDR_MAGIC_COMPRESSION | JOURNAL_COMPRESSION_LZ4;
    } else {
        ctx->pre_init.hdr_magic = DEFAULT_HDR_MAGIC;
    }

    return 0;
}

journal_t *journal_new(const char *path)
{
    journal_t *j = calloc(1, sizeof(*j));
    if (j == NULL)
        return NULL;

    j->file_mode = DEFAULT_FILE_MODE;

    j->meta.unit_limit = DEFAULT_UNIT_LIMIT;
    j->meta.safety = DEFAULT_SAFETY;
    j->meta.hdr_magic = DEFAULT_HDR_MAGIC;

    j->path = strdup(path);

    return j;
}

void journal_close(journal_t *j)
{
    free(j->path);
    free(j);
}

strlist_t *journal_get_threads(journal_t *journal)
{
    pthread_mutex_lock(&journal->lock);

    if (journal->threads == NULL) {
        pthread_mutex_unlock(&journal->lock);
        return NULL;
    }

    int size = 0;
    for (journal_thread_t *piv = journal->threads; piv != NULL; piv = piv->next) {
        size++;
    }

    strlist_t *sl = strlist_alloc(size);
    if (sl == NULL) {
        pthread_mutex_unlock(&journal->lock);
        return NULL;
    }

    for (journal_thread_t *piv = journal->threads; piv != NULL; piv = piv->next) {
        strlist_append(sl, piv->name);
    }

    pthread_mutex_unlock(&journal->lock);

    return sl;
}

int journal_thread_start(journal_t *journal, journal_thread_t *thread, char *name,
                                       void *(*start_routine)(void *), void *arg)
{
    thread->name = name;
    thread->loop = true;

    pthread_mutex_lock(&journal->lock);

    char thread_name[THREAD_NAME_MAX];
    ssnprintf(thread_name, sizeof(thread_name), "%s", name);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    set_thread_setaffinity(&attr, thread_name);

    int status = pthread_create(&thread->thread, &attr, start_routine, arg);
    pthread_attr_destroy(&attr);
    if (status != 0) {
        ERROR("pthread_create failed with status %i: %s.", status, STRERROR(status));
        pthread_mutex_unlock(&journal->lock);
        return status;
    }

    set_thread_name(thread->thread, thread_name);

    thread->next = journal->threads;
    journal->threads = thread;

    pthread_mutex_unlock(&journal->lock);

    return status;
}

int journal_thread_stop(journal_t *journal, const char *name)
{
    pthread_mutex_lock(&journal->lock);

    /* Build to completely new thread lists. One with threads to_stop and another
     * with threads to_keep. If name is NULL to_keep will be empty and to_stop
     * will contain all threads. If name is NULL to_stop will contain the
     * relevant thread and to_keep will contain all remaining threads. */
    journal_thread_t *to_stop = NULL;
    journal_thread_t *to_keep = NULL;

    for (journal_thread_t *piv = journal->threads; piv != NULL;) {
        journal_thread_t *next = piv->next;

        if ((name == NULL) || (strcasecmp(name, piv->name) == 0)) {
            piv->loop = false;
            piv->next = to_stop;
            to_stop = piv;
        } else {
            piv->next = to_keep;
            to_keep = piv;
        }

        piv = next;
    }

    journal->threads = to_keep;

    pthread_cond_broadcast(&journal->cond);
    pthread_mutex_unlock(&journal->lock);

    /* Return error if the requested thread was not found */
    if ((to_stop == NULL) && (name != NULL))
        return ENOENT;

    int status = 0;

    while (to_stop != NULL) {
        /* coverity[MISSING_LOCK] */
        journal_thread_t *next = to_stop->next;

        int ret = pthread_join(to_stop->thread, NULL);
        if (ret != 0) {
            ERROR("pthread_join failed for %s.", to_stop->name);
            status = ret;
        }

        free(to_stop->name);

        if (journal->free_thread_cb)
            journal->free_thread_cb(to_stop);
        else
            free(to_stop);
        to_stop = next;
    }

    return status;
}

