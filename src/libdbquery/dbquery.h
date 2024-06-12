/* SPDX-License-Identifier: GPL-2.0-only OR MIT                          */
/* SPDX-FileCopyrightText: Copyright (C) 2008,2009  Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>     */

#pragma once
struct db_query_s;
typedef struct db_query_s db_query_t;

struct db_query_preparation_area_s;
typedef struct db_query_preparation_area_s db_query_preparation_area_t;

typedef int (*db_query_create_callback_t)(db_query_t *q, config_item_t *ci);

int db_query_create(db_query_t ***ret_query_list, size_t *ret_query_list_len,
                    config_item_t *ci, db_query_create_callback_t cb);

void db_query_free(db_query_t **query_list, size_t query_list_len);

int db_query_pick_from_list_by_name(const char *name, db_query_t **src_list,
                                    size_t src_list_len,
                                    db_query_t ***dst_list,
                                    size_t *dst_list_len);

int db_query_pick_from_list(config_item_t *ci, db_query_t **src_list,
                            size_t src_list_len, db_query_t ***dst_list,
                            size_t *dst_list_len);

const char *db_query_get_name(db_query_t *q);

char *db_query_get_statement(db_query_t *q);

void db_query_preparation_area_set_user_data(db_query_preparation_area_t *q, void *user_data);

void *db_query_preparation_area_get_user_data(db_query_preparation_area_t *q);

/*
 * db_query_check_version
 *
 * Returns 0 if the query is NOT suitable for `version' and >0 if the
 * query IS suitable.
 */
int db_query_check_version(db_query_t *q, unsigned int version);

int db_query_prepare_result(db_query_t const *q, db_query_preparation_area_t *prep_area,
                                                 char *metric_prefix, label_set_t *labels,
                                                 const char *db_name, char **column_names,
                                                 size_t column_num);

int db_query_handle_result(db_query_t const *q, db_query_preparation_area_t *prep_area,
                                                char **column_values, plugin_filter_t *filter);
void db_query_finish_result(db_query_t const *q, db_query_preparation_area_t *prep_area);

db_query_preparation_area_t * db_query_allocate_preparation_area(db_query_t *q);

void db_query_delete_preparation_area(db_query_preparation_area_t *q_area);
