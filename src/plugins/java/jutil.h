/* SPDX-License-Identifier: GPL-2.0-only                                   */
/* SPDX-FileCopyrightText: Copyright (C) 2009  Florian octo Forster        */
/* SPDX-FileCopyrightText: Copyright (C) 2008  Justo Alonso Achaques       */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>       */
/* SPDX-FileContributor: Justo Alonso Achaques <justo.alonso at gmail.com> */

#pragma once

typedef struct {
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID add;
    } array_list;
    struct {
        jclass class;
        jmethodID size;
        jmethodID to_array;
    } list;   
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID put;
    } hash_map;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID boolean_value;
    } boolean;
    struct {
        jclass class;
        jmethodID constructor;
    } _long;
    struct {
        jclass class;
        jmethodID constructor;
    } _double;
    struct {
        jclass class;
        jmethodID entry_set;
    } map;
    struct {
        jclass class;
        jmethodID iterator;
    } set;
    struct {
        jclass class;
        jmethodID has_next;
        jmethodID next;
    } iterator;
    struct {
        jclass class;
        jmethodID get_key;
        jmethodID get_value;
    } map_entry;
    struct {
        jclass class;
        jmethodID set_labels;
        jmethodID set_time;
        jmethodID set_interval;
        jmethodID get_labels;
        jmethodID get_time;
        jmethodID get_interval;
    } metric;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID set_name;
        jmethodID set_help;
        jmethodID set_unit;
        jmethodID add_metric;
        jmethodID get_name;
        jmethodID get_help;
        jmethodID get_unit;
        jmethodID get_type;
        jmethodID get_metrics;
    } metric_family;
    struct {
        jclass class;
        jmethodID constructor_long;
        jmethodID constructor_double;
        jmethodID get_type;
        jmethodID get_long;
        jmethodID get_double;
    } metric_unknown;
    struct {
        jclass class;
        jmethodID constructor_long;
        jmethodID constructor_double;
        jmethodID get_type;
        jmethodID get_long;
        jmethodID get_double;
    } metric_gauge;
    struct {
        jclass class;
        jmethodID constructor_long;
        jmethodID constructor_double;
        jmethodID get_type;
        jmethodID get_long;
        jmethodID get_double;
    } metric_counter;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID get_info;
    } metric_info;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID get_set;
    } metric_state_set;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID get_sum;
        jmethodID get_buckets;
    } metric_histogram;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID get_counter;
        jmethodID get_maximum;
    } metric_histogram_bucket;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID get_sum;
        jmethodID get_count;
        jmethodID get_quantiles;
    } metric_summary;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID get_quantile;
        jmethodID get_value;
    } metric_summary_quantile;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID set_name;
        jmethodID set_time;
        jmethodID set_severity;
        jmethodID set_labels;
        jmethodID set_annotations;
        jmethodID get_name; 
        jmethodID get_time;
        jmethodID get_severity;
        jmethodID get_labels;
        jmethodID get_annotations;
    } notification;
    struct {
        jclass class;
        jmethodID constructor_bool;
        jmethodID constructor_string;
        jmethodID constructor_number;
    } config_value;
    struct {
        jclass class;
        jmethodID constructor;
        jmethodID add_value;
        jmethodID add_child;
    } config_item;
} jcached_refs_t;

int cjni_cache_classes(JNIEnv *jvm_env);

void cjni_cache_classes_release(JNIEnv *jvm_env);

int ctoj_string(JNIEnv *jvm_env, const char *string, jobject object_ptr, jmethodID method_id);

jobject ctoj_jlong_to_number(JNIEnv *jvm_env, jlong value);

jobject ctoj_jdouble_to_number(JNIEnv *jvm_env, jdouble value);

jobject ctoj_label_set_object(JNIEnv *jvm_env, const label_set_t *label);

jobject ctoj_state_set_object(JNIEnv *jvm_env, const state_set_t *set);

jobject ctoj_metric_family(JNIEnv *jvm_env, const metric_family_t *fam);

int jtoc_string(JNIEnv *jvm_env, char **str, jobject object_ptr, jmethodID method_id);

int jtoc_label_set(JNIEnv *jvm_env, label_set_t *label, jobject object_ptr, jmethodID method_id);

int jtoc_state_set(JNIEnv *jvm_env, state_set_t *set, jobject object_ptr, jmethodID method_id);

int jtoc_metric_family(JNIEnv *jvm_env, metric_family_t *fam, jobject o_fam);

jobject ctoj_notification(JNIEnv *jvm_env, const notification_t *n);

int jtoc_notification(JNIEnv *jvm_env, notification_t *n, jobject object_ptr);

jobject ctoj_config_value(JNIEnv *jvm_env, config_value_t ocvalue);

jobject ctoj_config_item(JNIEnv *jvm_env, const config_item_t *ci);

