/* SPDX-License-Identifier: GPL-2.0-only OR BSD-3-Clause                  */
/* SPDX-FileCopyrightText: Copyright (c) 2005-2008, Message Systems, Inc. */

#pragma once

#include "libutils/avltree.h"
#include "libutils/complain.h"

typedef enum {
    JOURNAL_NEW = 0,
    JOURNAL_INIT,
    JOURNAL_READ,
    JOURNAL_APPEND,
    JOURNAL_INVALID
} journal_mode_t;

typedef enum {
    JOURNAL_BEGIN,
    JOURNAL_END
} journal_position_t;

typedef enum {
    JOURNAL_UNSAFE,
    JOURNAL_ALMOST_SAFE,
    JOURNAL_SAFE
} journal_safety_t;

typedef enum {
    JOURNAL_ERR_SUCCESS = 0,
    JOURNAL_ERR_ILLEGAL_INIT,
    JOURNAL_ERR_ILLEGAL_OPEN,
    JOURNAL_ERR_OPEN,
    JOURNAL_ERR_NOTDIR,
    JOURNAL_ERR_CREATE_PATHLEN,
    JOURNAL_ERR_CREATE_EXISTS,
    JOURNAL_ERR_CREATE_MKDIR,
    JOURNAL_ERR_CREATE_META,
    JOURNAL_ERR_CREATE_PRE_COMMIT,
    JOURNAL_ERR_LOCK,
    JOURNAL_ERR_IDX_OPEN,
    JOURNAL_ERR_IDX_SEEK,
    JOURNAL_ERR_IDX_CORRUPT,
    JOURNAL_ERR_IDX_WRITE,
    JOURNAL_ERR_IDX_READ,
    JOURNAL_ERR_FILE_OPEN,
    JOURNAL_ERR_FILE_SEEK,
    JOURNAL_ERR_FILE_CORRUPT,
    JOURNAL_ERR_FILE_READ,
    JOURNAL_ERR_FILE_WRITE,
    JOURNAL_ERR_META_OPEN,
    JOURNAL_ERR_PRE_COMMIT_OPEN,
    JOURNAL_ERR_ILLEGAL_WRITE,
    JOURNAL_ERR_ILLEGAL_CHECKPOINT,
    JOURNAL_ERR_INVALID_SUBSCRIBER,
    JOURNAL_ERR_ILLEGAL_LOGID,
    JOURNAL_ERR_SUBSCRIBER_EXISTS,
    JOURNAL_ERR_CHECKPOINT,
    JOURNAL_ERR_NOT_SUPPORTED,
    JOURNAL_ERR_CLOSE_LOGID
} journal_err_t;

typedef enum {
    JOURNAL_COMPRESSION_NULL = 0x00,
    JOURNAL_COMPRESSION_LZ4  = 0x01
} journal_compression_t;

typedef enum {
    JOURNAL_READ_METHOD_MMAP = 0,
    JOURNAL_READ_METHOD_PREAD
} journal_read_method_type_t;

typedef struct {
    dev_t st_dev;
    ino_t st_ino;
} journal_file_id_t;

typedef struct {
    journal_file_id_t id;
    int fd;
    int refcnt;
    int locked;
    pthread_mutex_t lock;
} journal_file_t;

typedef struct {
    uint32_t storage_log;
    uint32_t unit_limit;
    uint32_t safety;
    uint32_t hdr_magic;
} journal_meta_info_t;

struct journal_thread_s;
typedef struct journal_thread_s journal_thread_t;

typedef struct {
    const char *kind;

    char *path;
    int  file_mode;

    bool checksum;
    int compress;
    off_t retention_size;
    cdtime_t retention_time;
    off_t segment_size;

    journal_meta_info_t meta;

    int   last_error;
    int   last_errno;

    void (*free_thread_cb)(void *);

    c_avl_tree_t *files;

    c_complain_t complaint;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    journal_thread_t *threads;

} journal_t;

typedef struct {
    journal_t *journal;

    journal_meta_info_t *meta;
    pthread_mutex_t write_lock;
    journal_read_method_type_t read_method;
    int       meta_is_mapped;
    journal_meta_info_t pre_init; /* only used before we're opened */
    journal_mode_t context_mode;
    char     *path;
    int       file_mode;
    uint32_t current_log;
    journal_file_t *data;
    journal_file_t *index;
    journal_file_t *checkpoint;
    journal_file_t *metastore;
    void     *mmap_base;
    size_t    mmap_len;
    size_t    data_file_size;
    uint8_t   reader_is_initialized;
    void     *compressed_data_buffer;
    size_t    compressed_data_buffer_len;
    char     *subscriber_name;
    int       last_error;
    int       last_errno;

    /**
     * Store the last read message in the case of use_compression == 1.
     * There is an expectation from journal user's that they are handed
     * mmap'd memory directly and that they don't have to free it, so we
     * have to maintain a block of memory where we can decompress messages
     * and hand them back.  The contract being that everytime journal_ctx_read_message
     * is called, we overwrite this memory with the new message
     */
    size_t    mess_data_size;
    char      *mess_data;
} journal_ctx_t;

struct journal_thread_s {
    char *name;
    bool loop;
    pthread_t thread;
    journal_ctx_t *ctx;
    journal_thread_t *next;
};

typedef struct {
    uint32_t reserved;
    uint32_t tv_sec;
    uint32_t tv_usec;
    uint32_t mlen;
} journal_message_header_t;

typedef struct {
    uint32_t reserved;
    uint32_t tv_sec;
    uint32_t tv_usec;
    uint32_t mlen;
    uint32_t compressed_len;
} journal_message_header_compressed_t;

typedef struct {
    uint32_t log;
    uint32_t marker;
} journal_id_t;

#define JOURNAL_ID_ADVANCE(id) (id)->marker++

typedef struct {
    journal_message_header_compressed_t *header;
    uint32_t mess_len;
    void *mess;
    journal_message_header_compressed_t aligned_header;
} journal_message_t;

journal_t *journal_new(const char *path);
void journal_close(journal_t *j);

int journal_init(journal_t *j);

int journal_config_checksum(journal_t *j, bool checksum);
int journal_config_compress(journal_t *j, bool compress);
int journal_config_retention_size(journal_t *j, off_t retention_size);
int journal_config_retention_time(journal_t *j, cdtime_t retention_time);
int journal_config_segment_size(journal_t *j, off_t segment_size);

journal_ctx_t *journal_get_writer(journal_t *j);
journal_ctx_t *journal_get_reader(journal_t *j, const char *subscriber);

int journal_pending_readers(journal_t *j, uint32_t log, uint32_t *earliest_ptr);

int journal_remove_subscriber(journal_t *j, const char *subscriber);

int journal_ctx_add_subscriber(journal_ctx_t *ctx, const char *subscriber,
                                                   journal_position_t whence);
int journal_ctx_add_subscriber_copy_checkpoint(journal_ctx_t *ctx, const char *new_subscriber,
                                                                   const char *old_subscriber);
int journal_ctx_set_subscriber_checkpoint(journal_ctx_t *ctx, const char *subscriber,
                                                              const journal_id_t *checkpoint);



const char *journal_err_string(int);

size_t journal_ctx_raw_size(journal_ctx_t *ctx);
int journal_ctx_get_checkpoint(journal_ctx_t *ctx, const char *s, journal_id_t *id);
int journal_ctx_list_subscribers_dispose(journal_ctx_t *ctx, char **subs);
int journal_ctx_list_subscribers(journal_ctx_t *ctx, char ***subs);

int journal_ctx_err(journal_ctx_t *ctx);
const char *journal_ctx_err_string(journal_ctx_t *ctx);
int journal_ctx_errno(journal_ctx_t *ctx);

int journal_ctx_close(journal_ctx_t *ctx);

int journal_ctx_alter_mode(journal_ctx_t *ctx, int mode);
int journal_ctx_alter_journal_size(journal_ctx_t *ctx, size_t size);
int journal_ctx_alter_safety(journal_ctx_t *ctx, journal_safety_t safety);
int journal_ctx_alter_read_method(journal_ctx_t *ctx, journal_read_method_type_t method);

int journal_ctx_repair(journal_ctx_t *ctx, int aggressive);

int journal_ctx_set_use_compression(journal_ctx_t *ctx, bool use);

int journal_ctx_write(journal_ctx_t *ctx, void *message, size_t mess_len);
int journal_ctx_write_message(journal_ctx_t *ctx, journal_message_t *msg, struct timeval *when);
int journal_ctx_read_interval(journal_ctx_t *ctx, journal_id_t *first_mess, journal_id_t *last_mess);
int journal_ctx_read_message(journal_ctx_t *ctx, const journal_id_t *, journal_message_t *);
int journal_ctx_read_checkpoint(journal_ctx_t *ctx, const journal_id_t *checkpoint);

int journal_snprint_logid(char *buff, int n, const journal_id_t *checkpoint);

int journal_ctx_first_log_id(journal_ctx_t *ctx, journal_id_t *id);
int journal_ctx_last_log_id(journal_ctx_t *ctx, journal_id_t *id);
int journal_ctx_last_storage_log(journal_ctx_t *ctx, uint32_t *logid);
int journal_ctx_advance_id(journal_ctx_t *ctx, journal_id_t *cur, journal_id_t *start, journal_id_t *finish);
int journal_clean(const char *path);


int journal_thread_start(journal_t *journal, journal_thread_t *thread, char *name,
                                       void *(*start_routine)(void *), void *arg);

int journal_thread_stop(journal_t *journal, const char *name);

strlist_t *journal_get_threads(journal_t *journal);

