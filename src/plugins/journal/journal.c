// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <systemd/sd-journal.h>

typedef struct {
    char *name;
    char *unit;
    char *namespace;
    char *path;
    label_set_t labels;
    sd_journal *journal;
    plugin_filter_t *filter;
    plugin_match_t *matches;
    pthread_t thread_id;
    pthread_mutex_t lock;
    bool thread_running;
} journal_t;

static size_t journal_get_data(sd_journal *j, char *label,
                                              char *prefix, size_t prefix_len,
                                              char *buffer, size_t buffer_len)
{
    const char *data;
    size_t data_len;

    int status = sd_journal_get_data(j, label, (const void **)&data, &data_len);
    if (status != 0)
        return 0;

    if (strncmp(prefix, data, prefix_len) != 0)
        return 0;
    data += prefix_len;
    data_len -= prefix_len;

    if (data_len > (buffer_len - 1))
        data_len = buffer_len - 1;
    memcpy(buffer, data, data_len);
    buffer[data_len] = '\0';

    return data_len;
}

static void *journal_thread_read(void *data)
{
    journal_t *ctx = data;

    if (ctx->journal == NULL) {
        int status = 0;

        if (ctx->namespace != NULL) {
            status = sd_journal_open_namespace(&ctx->journal, ctx->namespace, 0);
        } else if (ctx->path != NULL) {
            status = sd_journal_open_directory(&ctx->journal, ctx->path, 0);
        } else {
            status = sd_journal_open(&ctx->journal, SD_JOURNAL_LOCAL_ONLY);
        }

        if (status != 0) {
            PLUGIN_ERROR("Failed to open journal: %s.", STRERROR(-status));
            pthread_exit(NULL);
        }

        if (ctx->unit != NULL) {
            char filter[256];
            ssnprintf(filter, sizeof(filter), "_SYSTEMD_UNIT=%s", ctx->unit);
            sd_journal_add_match(ctx->journal, filter, 0);
        }

        status = sd_journal_seek_tail(ctx->journal);
        if (status != 0) {
            PLUGIN_ERROR("Failed to seek to the tail of the journal.");
        }

        sd_journal_previous(ctx->journal);
    }


    while (ctx->thread_running) {
        int status = sd_journal_next(ctx->journal);
        if (status == 0) {
            status = sd_journal_wait(ctx->journal, (uint64_t) 1000000);
            if (status < 0)
                PLUGIN_ERROR("Failed to wait for changes: %s\n", STRERROR(-status));
            continue;
        }

        if (status < 0) {
            PLUGIN_ERROR("Failed to read the next message in the journal.");
            break;
        }

        char message[4096];
        size_t message_len = journal_get_data(ctx->journal, "MESSAGE",
                                                      "MESSAGE=", strlen("MESSAGE="),
                                                       message, sizeof(message));
        if (message_len == 0)
            continue;

        pthread_mutex_lock(&ctx->lock);
        status = plugin_match(ctx->matches, message);
        pthread_mutex_unlock(&ctx->lock);
        if (status != 0)
            PLUGIN_WARNING("plugin_match failed.");
    }


    pthread_exit(NULL);
}

static int journal_read(user_data_t *ud)
{
    journal_t *ctx = ud->data;

    pthread_mutex_lock(&ctx->lock);

    plugin_match_dispatch(ctx->matches, ctx->filter, &ctx->labels, true);

    pthread_mutex_unlock(&ctx->lock);

    return 0;
}

static void journal_free(void *arg)
{
    journal_t *journal  = arg;

    if (journal == NULL)
        return;

    if (journal->thread_running) {
        journal->thread_running = false;
        pthread_join(journal->thread_id, NULL);
    }

    pthread_mutex_destroy(&journal->lock);

    free(journal->name);
    free(journal->unit);
    free(journal->namespace);
    free(journal->path);

    label_set_reset(&journal->labels);

    if (journal->journal != NULL)
        sd_journal_close(journal->journal);

    if (journal->matches != NULL)
        plugin_match_shutdown(journal->matches);

    plugin_filter_free(journal->filter);

    free(journal);
}

static int journal_config_instance(config_item_t *ci)
{
    cdtime_t interval = 0;

    char *name= NULL;
    int status = cf_util_get_string(ci, &name);
    if (status != 0)
        return status;

    journal_t *journal  = calloc(1, sizeof(*journal));
    if (journal == NULL)
        return -1;
    journal->name = name;

    pthread_mutex_init(&journal->lock, NULL);

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("unit", child->key) == 0) {
            status = cf_util_get_string(child, &journal->unit);
        } else if (strcasecmp("namespace", child->key) == 0) {
            status = cf_util_get_string(child, &journal->namespace);
        } else if (strcasecmp("path", child->key) == 0) {
            status = cf_util_get_string(child, &journal->path);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &journal->labels);
        } else if (strcasecmp("match", child->key) == 0) {
            status = plugin_match_config(child, &journal->matches);
        } else if (strcasecmp(child->key, "filter") == 0) {
            status = plugin_filter_configure(child, &journal->filter);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                          child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status == 0) {
        if (journal->matches == NULL) {
            PLUGIN_ERROR("No (valid) 'match' block ");
            status = -1;
        }
    }

    if (status != 0) {
        journal_free(journal);
        return -1;
    }

    label_set_add(&journal->labels, true, "instance", journal->name);

    journal->thread_running = true;

    status = plugin_thread_create(&journal->thread_id, journal_thread_read, journal, "journal");
    if (status != 0) {
        PLUGIN_ERROR("pthread_create() failed.");
        journal_free(journal);
        return -1;
    }

    return plugin_register_complex_read("journal", name, journal_read, interval,
                                        &(user_data_t){.data=journal, .free_func=journal_free});
}

static int journal_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = journal_config_instance(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' in %s:%d is not allowed here.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

void module_register(void)
{
    plugin_register_config("journal", journal_config);
}
