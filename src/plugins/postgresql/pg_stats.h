/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

int pg_stat_database(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                   char *db);
int pg_database_size(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                   char *db);
int pg_database_locks(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                    char *db);
int pg_stat_user_table(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                     char *schema, char *table);
int pg_statio_user_tables(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                        char *schema, char *table);
int pg_stat_user_indexes(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                       char *schema, char *table, char *index);
int pg_statio_user_indexes(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                         char *schema, char *table, char *index);
int pg_statio_user_sequences(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                           char *schema, char *sequence);
int pg_stat_user_functions(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                         char *schema, char *function);
int pg_stat_activity (PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                    char *db);
int pg_replication_slots(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels,
                                       char *db);
int pg_stat_replication(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels);

int pg_stat_archiver(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels);

int pg_stat_bgwriter(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels);

int pg_stat_checkpointer(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels);

int pg_stat_slru(PGconn *conn, int version, metric_family_t *fams, label_set_t *labels);
