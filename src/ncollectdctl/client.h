/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

#include "libmetric/notification.h"
#include "libmdb/strlist.h"
#include "libmdb/series_list.h"
#include "libmdb/family_metric_list.h"

struct client_s;
typedef struct client_s client_t;

client_t *client_create(char *path);

void client_destroy(client_t *client);

strlist_t *client_get_plugins_readers(client_t *client);

strlist_t *client_get_plugins_writers(client_t *client);

strlist_t *client_get_plugins_loggers(client_t *client);

strlist_t *client_get_plugins_notificators(client_t *client);

int client_post_write(client_t *client, char *data, size_t data_len);

int client_post_notification(client_t *client, notification_t *n);

#if 0
client_read
client_write
client_query
client_query_range
#endif

mdb_series_list_t *client_get_series(client_t *client);

mdb_family_metric_list_t *client_get_family_metrics(client_t *client);

strlist_t *client_get_metrics(client_t *client);

strlist_t *client_get_metric_labels(client_t *client, char *metric);

strlist_t *client_get_metric_label_values(client_t *client, char *metric, char *label);
