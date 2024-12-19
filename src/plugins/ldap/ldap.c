// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2011 Kimo Rosenbaum
// SPDX-FileCopyrightText: Copyright (C) 2014-2015 Marc Fournier
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Kimo Rosenbaum <kimor79 at yahoo.com>
// SPDX-FileContributor: Marc Fournier <marc.fournier at camptocamp.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <lber.h>
#include <ldap.h>

// numSubordinates
// numAllSubordinates
// ldapentrycount

typedef struct {
    char *dn;
    char *cn;
    char *metric_prefix;
    char *metric;
    char *metric_from;
    char *help;
    char *help_from;
    metric_type_t type;
    label_set_t labels;
    label_set_t labels_from;
    char *value_from;
} ldap_metric_t;

typedef struct {
    char *name;
    char *base;
    int scope;
    char *filter;
    char **attrs;
    char *metric_prefix;
    label_set_t labels;
    label_set_t labels_from;
    size_t metrics_size;
    ldap_metric_t **metrics;
} ldap_query_t;

typedef struct {
    char *name;
    char *binddn;
    char *password;
    char *cacert;
    bool starttls;
    cdtime_t timeout;
    char *url;
    bool verifyhost;
    int version;
    char *metric_prefix;
    label_set_t labels;
    plugin_filter_t *filter;
    LDAP *ld;
    size_t queries_size;
    ldap_query_t **queries;
} ldap_t;

static size_t global_queries_size;
static ldap_query_t **global_queries;

static void ldap_metric_free(ldap_metric_t *metric)
{
    if (metric == NULL)
        return;

    free(metric->dn);
    free(metric->cn);
    free(metric->metric);
    free(metric->metric_from);
    free(metric->help);
    free(metric->help_from);
    label_set_reset(&metric->labels);
    label_set_reset(&metric->labels_from);
    free(metric->value_from);

    free(metric );
}

static void ldap_query_free(ldap_query_t *query)
{
    if (query == NULL)
        return;

    free(query->name);
    free(query->base);
    free(query->filter);

    if (query->attrs != NULL) {
        for (size_t i = 0; query->attrs[i] != NULL; i++)
            free(query->attrs[i]);
        free(query->attrs);
    }

    label_set_reset(&query->labels);
    label_set_reset(&query->labels_from);

    if (query->metrics != NULL) {
        for (size_t i = 0; i < query->metrics_size; i++) {
            ldap_metric_free(query->metrics[i]);
        }
        free(query->metrics);
    }

    free(query);
}

static void ldap_free(void *arg)
{
    ldap_t *st = arg;

    if (st == NULL)
        return;

    free(st->name);
    free(st->binddn);
    free(st->password);
    free(st->cacert);
    free(st->url);
    if (st->ld)
        ldap_unbind_ext_s(st->ld, NULL, NULL);

    label_set_reset(&st->labels);
    plugin_filter_free(st->filter);

    free(st->queries);

    free(st);
}

static int ldap_init_host(ldap_t *st)
{
    int rc;

    if (unlikely(st->ld)) {
        PLUGIN_DEBUG("Already connected to %s", st->url);
        return 0;
    }

    rc = ldap_initialize(&st->ld, st->url);
    if (unlikely(rc != LDAP_SUCCESS)) {
        PLUGIN_ERROR("ldap_initialize failed: %s", ldap_err2string(rc));
        if (st->ld != NULL)
            ldap_unbind_ext_s(st->ld, NULL, NULL);
        st->ld = NULL;
        return -1;
    }

    ldap_set_option(st->ld, LDAP_OPT_PROTOCOL_VERSION, &st->version);

    struct timeval tm = CDTIME_T_TO_TIMEVAL(st->timeout);
    ldap_set_option(st->ld, LDAP_OPT_TIMEOUT, &tm);

    ldap_set_option(st->ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

    if (st->cacert != NULL)
        ldap_set_option(st->ld, LDAP_OPT_X_TLS_CACERTFILE, st->cacert);

    if (st->verifyhost == false) {
        int never = LDAP_OPT_X_TLS_NEVER;
        ldap_set_option(st->ld, LDAP_OPT_X_TLS_REQUIRE_CERT, &never);
    }

    if (st->starttls) {
        rc = ldap_start_tls_s(st->ld, NULL, NULL);
        if (rc != LDAP_SUCCESS) {
            PLUGIN_ERROR("Failed to start tls on %s: %s", st->url, ldap_err2string(rc));
            ldap_unbind_ext_s(st->ld, NULL, NULL);
            st->ld = NULL;
            return -1;
        }
    }

    struct berval cred;
    if (st->password != NULL) {
        cred.bv_val = st->password;
        cred.bv_len = strlen(st->password);
    } else {
        cred.bv_val = "";
        cred.bv_len = 0;
    }

    rc = ldap_sasl_bind_s(st->ld, st->binddn, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);
    if (unlikely(rc != LDAP_SUCCESS)) {
        PLUGIN_ERROR("Failed to bind to %s: %s", st->url, ldap_err2string(rc));
        ldap_unbind_ext_s(st->ld, NULL, NULL);
        st->ld = NULL;
        return -1;
    }

    PLUGIN_DEBUG("Successfully connected to %s", st->url);
    return 0;
}

static int ldap_submit(ldap_t *ldap, ldap_query_t *query, ldap_metric_t *metric, LDAPMessage *e)
{
    metric_family_t fam = {0};
    metric_t m = {0};
    struct berval **help_list = NULL;
    struct berval **value_list = NULL;
    int status = 0;

    fam.type = metric->type;

    strbuf_t buf = STRBUF_CREATE;

    if (ldap->metric_prefix != NULL)
        strbuf_putstr(&buf, ldap->metric_prefix);
    if (query->metric_prefix != NULL)
        strbuf_putstr(&buf, query->metric_prefix);
    if (metric->metric_prefix != NULL)
        strbuf_putstr(&buf, metric->metric_prefix);

    if (metric->metric_from != NULL) {
        struct berval **list = ldap_get_values_len(ldap->ld, e, metric->metric_from);
        if (list == NULL) {
            PLUGIN_ERROR("Cannot find attribute '%s'.", metric->metric_from);
            status = -1;
            goto error;
        }
        strbuf_putstr(&buf, list[0]->bv_val);
        ldap_value_free_len(list);
    } else {
        strbuf_putstr(&buf, metric->metric);
    }

    fam.name = buf.ptr;

    if (metric->help_from != NULL) {
        help_list = ldap_get_values_len(ldap->ld, e, metric->help_from);
        if (help_list == NULL) {
            PLUGIN_ERROR("Cannot find attribute '%s'.", metric->help_from);
            status = -1;
            goto error;
        }
        fam.help = help_list[0]->bv_val;
    } else if (metric->help != NULL) {
        fam.help = metric->help;
    }

    for (size_t i = 0; i < ldap->labels.num; i++) {
        metric_label_set(&m, ldap->labels.ptr[i].name, ldap->labels.ptr[i].value);
    }

    for (size_t i = 0; i < query->labels.num; i++) {
        metric_label_set(&m, query->labels.ptr[i].name, query->labels.ptr[i].value);
    }

    for (size_t i = 0; i < query->labels_from.num; i++) {
        struct berval **list = ldap_get_values_len(ldap->ld, e, query->labels_from.ptr[i].value);
        if (list == NULL) {
            PLUGIN_ERROR("Cannot find attribute '%s'.", query->labels_from.ptr[i].value);
            status = -1;
            goto error;
        }
        metric_label_set(&m, query->labels_from.ptr[i].name, list[0]->bv_val);
        ldap_value_free_len(list);
    }

    for (size_t i = 0; i < metric->labels.num; i++) {
        metric_label_set(&m, metric->labels.ptr[i].name, metric->labels.ptr[i].value);
    }

    for (size_t i = 0; i < metric->labels_from.num; i++) {
        struct berval **list = ldap_get_values_len(ldap->ld, e, metric->labels_from.ptr[i].value);
        if (list == NULL) {
            PLUGIN_ERROR("Cannot find attribute '%s'.", metric->labels_from.ptr[i].value);
            status = -1;
            goto error;
        }
        metric_label_set(&m, metric->labels_from.ptr[i].name, list[0]->bv_val);
        ldap_value_free_len(list);
    }

    value_list = ldap_get_values_len(ldap->ld, e, metric->value_from);
    if (value_list == NULL) {
        PLUGIN_ERROR("Cannot find attribute '%s'.", metric->value_from);
        status = -1;
        goto error;
    }

    char *value_str = value_list[0]->bv_val;
    if (fam.type == METRIC_TYPE_GAUGE) {
        double value;
        if (0 != parse_double(value_str, &value)) {
            PLUGIN_ERROR("Parsing '%s' as gauge failed.", value_str);
            status = -1;
            goto error;
        }
        m.value.gauge.float64 = value;
    } else if (fam.type == METRIC_TYPE_COUNTER) {
        uint64_t value;
        if (0 != parse_uinteger(value_str, &value)) {
            PLUGIN_ERROR("Parsing '%s' as counter failed.", value_str);
            status = -1;
            goto error;
        }
        m.value.counter.uint64 = value;
    }

    metric_family_metric_append(&fam, m);
    plugin_dispatch_metric_family_filtered(&fam, ldap->filter, 0);

error:
    metric_reset(&m, fam.type);
    metric_family_metric_reset(&fam);
    strbuf_destroy(&buf);
    if (help_list != NULL)
        ldap_value_free_len(help_list);
    if (value_list != NULL)
        ldap_value_free_len(value_list);

    return status;
}

static int ldap_read_host(user_data_t *ud)
{
    if (unlikely((ud == NULL) || (ud->data == NULL))) {
        PLUGIN_ERROR("Invalid user data.");
        return -1;
    }

    ldap_t *ldap = (ldap_t *)ud->data;

    int status = ldap_init_host(ldap);
    if (unlikely(status != 0))
        return -1;

    for (size_t i = 0; i < ldap->queries_size; i++) {
        ldap_query_t *query = ldap->queries[i];

        char *filter = query->filter != NULL ? query->filter : "(objectClass=*)";

        LDAPMessage *result;
        int rc = ldap_search_ext_s(ldap->ld, query->base, query->scope, filter,
                                   query->attrs, 0, NULL, NULL, NULL, 0, &result);

        if (unlikely(rc != LDAP_SUCCESS)) {
            PLUGIN_ERROR("Failed to execute search: %s", ldap_err2string(rc));
            ldap_msgfree(result);
            ldap_unbind_ext_s(ldap->ld, NULL, NULL);
            ldap->ld = NULL;
            return -1;
        }

        for (LDAPMessage *e = ldap_first_entry(ldap->ld, result);
             e != NULL;
             e = ldap_next_entry(ldap->ld, e)) {

            char *dn = ldap_get_dn(ldap->ld, e);

            struct berval **cn_list = ldap_get_values_len(ldap->ld, e, "cn");

            for (size_t j = 0; j < query->metrics_size; j++) {
                ldap_metric_t *metric = query->metrics[i];

                if ((dn != NULL) && (metric->dn != NULL) && (strcmp(dn, metric->dn) != 0))
                    continue;

                if ((cn_list != NULL) && (metric->cn != NULL) &&
                    (strcmp((*cn_list[0]).bv_val, metric->cn) != 0))
                    continue;

                ldap_submit(ldap, query, metric, e);
            }

            if (dn != NULL)
                ldap_memfree(dn);
            if (cn_list !=NULL)
                ldap_value_free_len(cn_list);
        }

        ldap_msgfree(result);
    }

    return 0;
}

static int ldap_config_metric(config_item_t *ci, ldap_query_t *query)
{
    ldap_metric_t *metric = calloc(1, sizeof(*metric));
    if (metric == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("dn", child->key) == 0) {
            status = cf_util_get_string(child, &metric->dn);
        } else if (strcasecmp("cn", child->key) == 0) {
            status = cf_util_get_string(child, &metric->cn);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &metric->metric_prefix);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = cf_util_get_string(child, &metric->metric);
        } else if (strcasecmp("metric-from", child->key) == 0) {
            status = cf_util_get_string(child, &metric->metric_from);
        } else if (strcasecmp("help", child->key) == 0) {
            status = cf_util_get_string(child, &metric->help);
        } else if (strcasecmp("help-from", child->key) == 0) {
            status = cf_util_get_string(child, &metric->help_from);
        } else if (strcasecmp("type", child->key) == 0) {
            status = cf_util_get_metric_type(child, &metric->type);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &metric->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &metric->labels_from);
        } else if (strcasecmp("value-from", child->key) == 0) {
            status = cf_util_get_string(child, &metric->value_from);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ldap_metric_free(metric);
        return -1;
    }

    ldap_metric_t **tmp = realloc(query->metrics,
                                  sizeof(*(query->metrics)) * (query->metrics_size + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        ldap_metric_free(metric);
        return -1;
    }

    query->metrics = tmp;
    query->metrics[query->metrics_size] = metric;
    query->metrics_size++;

    return 0;
}

static int ldap_config_scope(config_item_t *ci, int *scope)
{
    if (ci == NULL)
        return -1;

    if ((ci->values_num != 1) || (ci->values[0].type != CONFIG_TYPE_STRING)) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires exactly one string argument.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    if (strcasecmp("base", ci->values[0].value.string) == 0) {
        *scope = LDAP_SCOPE_BASE;
    } else if (strcasecmp("one", ci->values[0].value.string) == 0) {
        *scope = LDAP_SCOPE_ONELEVEL;
    } else if (strcasecmp("sub", ci->values[0].value.string) == 0) {
        *scope = LDAP_SCOPE_SUBTREE;
    } else if (strcasecmp("children", ci->values[0].value.string) == 0) {
        *scope = LDAP_SCOPE_SUBORDINATE;
    } else {
        PLUGIN_ERROR("The '%s' option in %s:%d must be: 'base', 'one', 'sub' or 'children' ",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    return 0;
}

static int ldap_config_attrs(config_item_t *ci, char **attrs[])
{

    if ((ci == NULL) || (*attrs == NULL))
        return -1;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of strings.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }
    }

    *attrs = calloc(ci->values_num, sizeof(char *));
    if (*attrs == NULL) {
        PLUGIN_ERROR("calloc failed: %s.", STRERRNO);
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        (*attrs)[i] = strdup(ci->values[i].value.string);
        if ((*attrs)[i] == NULL)
            goto error;
    }

    return 0;

error:
    for (int i=0 ; i < ci->values_num; i++) {
        free((*attrs)[i]);
    }

    return -1;
}

static int ldap_config_query(config_item_t *ci)
{
    ldap_query_t *query = calloc(1, sizeof(*query));
    if (query == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &query->name);
    if (status != 0) {
        free(query);
        return status;
    }

    query->scope = LDAP_SCOPE_SUBTREE;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("base", child->key) == 0) {
            status = cf_util_get_string(child, &query->base);
        } else if (strcasecmp("scope", child->key) == 0) {
            status = ldap_config_scope(child, &query->scope);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = cf_util_get_string(child, &query->filter);
        } else if (strcasecmp("attrs", child->key) == 0) {
            status = ldap_config_attrs(child, &query->attrs);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &query->labels);
        } else if (strcasecmp("label-from", child->key) == 0) {
            status = cf_util_get_label(child, &query->labels_from);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &query->metric_prefix);
        } else if (strcasecmp("metric", child->key) == 0) {
            status = ldap_config_metric(child, query);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    if (status != 0) {
        ldap_query_free(query);
        return -1;
    }

    ldap_query_t **tmp = realloc(global_queries,
                                 sizeof(*(global_queries)) * (global_queries_size + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        ldap_query_free(query);
        return -1;
    }

    global_queries = tmp;
    global_queries[global_queries_size] = query;
    global_queries_size++;

    return 0;
}

static int ldap_config_add_query(config_item_t *ci, ldap_t *st)
{
    if ((ci == NULL) || (st == NULL))
        return -1;

    if (ci->values_num <= 0) {
        PLUGIN_ERROR("The '%s' option in %s:%d requires a list of strings.",
                     ci->key, cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    for (int i=0 ; i < ci->values_num; i++) {
        if (ci->values[i].type != CONFIG_TYPE_STRING) {
            PLUGIN_ERROR("The argument %d in option '%s' at %s:%d must be a string.",
                         i+1, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        ldap_query_t *query = NULL;
        for (size_t j=0 ; j < global_queries_size; j++) {
            if (strcmp(ci->values[i].value.string,  global_queries[j]->name) == 0) {
                query = global_queries[j];
                break;
            }
        }

        if (query == NULL) {
            PLUGIN_ERROR("Query name %s in option '%s' at %s:%d not found.",
                         ci->values[i].value.string, ci->key, cf_get_file(ci), cf_get_lineno(ci));
            return -1;
        }

        ldap_query_t **tmp = realloc(st->queries, sizeof(*tmp) * (st->queries_size + 1));
        if (tmp == NULL) {
            PLUGIN_ERROR("realloc failed.");
            return -1;
        }

        st->queries = tmp;
        st->queries[st->queries_size] = query;
        st->queries_size++;
    }

    return 0;
}

static int ldap_config_instance(config_item_t *ci)
{
    ldap_t *st = calloc(1, sizeof(*st));
    if (st == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    int status = cf_util_get_string(ci, &st->name);
    if (status != 0) {
        free(st);
        return status;
    }

    st->starttls = false;
    st->verifyhost = true;
    st->version = LDAP_VERSION3;

    cdtime_t interval = 0;
    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("bind-dn", child->key) == 0) {
            status = cf_util_get_string(child, &st->binddn);
        } else if (strcasecmp("password", child->key) == 0) {
            status = cf_util_get_string(child, &st->password);
        } else if (strcasecmp("password-env", child->key) == 0) {
            status = cf_util_get_string_env(child, &st->password);
        } else if (strcasecmp("ca-cert", child->key) == 0) {
            status = cf_util_get_string(child, &st->cacert);
        } else if (strcasecmp("start-tls", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->starttls);
        } else if (strcasecmp("timeout", child->key) == 0) {
            status = cf_util_get_cdtime(child, &st->timeout);
        } else if (strcasecmp("url", child->key) == 0) {
            status = cf_util_get_string(child, &st->url);
        } else if (strcasecmp("verify-host", child->key) == 0) {
            status = cf_util_get_boolean(child, &st->verifyhost);
        } else if (strcasecmp("version", child->key) == 0) {
            status = cf_util_get_int(child, &st->version);
        } else if (strcasecmp("interval", child->key) == 0) {
            status = cf_util_get_cdtime(child, &interval);
        } else if (strcasecmp("query", child->key) == 0) {
            status = ldap_config_add_query(child, st);
        } else if (strcasecmp("metric-prefix", child->key) == 0) {
            status = cf_util_get_string(child, &st->metric_prefix);
        } else if (strcasecmp("label", child->key) == 0) {
            status = cf_util_get_label(child, &st->labels);
        } else if (strcasecmp("filter", child->key) == 0) {
            status = plugin_filter_configure(child, &st->filter);
        } else {
            PLUGIN_WARNING("Option '%s' not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            break;
    }

    /* Check if struct is complete.. */
    if ((status == 0) && (st->url == NULL)) {
        PLUGIN_ERROR("Instance '%s': No 'url' has been configured.", st->name);
        status = -1;
    }

    /* Check if URL is valid */
    if ((status == 0) && (st->url != NULL)) {
        LDAPURLDesc *ludpp;

        if (ldap_url_parse(st->url, &ludpp) != 0) {
            PLUGIN_ERROR("Instance '%s': Invalid 'url': `%s'", st->name, st->url);
            status = -1;
        }

        ldap_free_urldesc(ludpp);
    }

    if (status != 0) {
        ldap_free(st);
        return -1;
    }

    if (st->timeout == 0)
        st->timeout = interval == 0 ? plugin_get_interval()/2 : interval/2;

    label_set_add(&st->labels, true, "instance", st->name);

    return plugin_register_complex_read("ldap", st->name, ldap_read_host, interval,
                                        &(user_data_t){ .data = st, .free_func = ldap_free });
}

static int cldap_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp("instance", child->key) == 0) {
            status = ldap_config_instance(child);
        } else if (strcasecmp("query", child->key) == 0) {
            status = ldap_config_query(child);
        } else {
            PLUGIN_ERROR("The configuration option '%s' is not allowed here.", child->key);
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int cldap_shutdown(void)
{
    if (global_queries != NULL) {
        for (size_t i=0; i < global_queries_size; i++) {
            ldap_query_free(global_queries[i]);
        }
        free(global_queries);

        global_queries_size = 0;
        global_queries = NULL;
    }

    return 0;
}

static int cldap_init(void)
{
    /* Initialize LDAP library while still single-threaded as recommended in
     * ldap_initialize(3) */
    int debug_level = 0;
    ldap_get_option(NULL, LDAP_OPT_DEBUG_LEVEL, &debug_level);
    return 0;
}

void module_register(void)
{
    plugin_register_config("ldap", cldap_config);
    plugin_register_init("ldap", cldap_init);
    plugin_register_shutdown("ldap", cldap_shutdown);
}
