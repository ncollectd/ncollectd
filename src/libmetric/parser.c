// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libutils/avltree.h"
#include "libmetric/metric.h"
#include "libmetric/metric_chars.h"
#include "libmetric/parser.h"

typedef enum {
    METRIC_COMMENT_HELP,
    METRIC_COMMENT_TYPE,
    METRIC_COMMENT_UNIT,
    METRIC_COMMENT_END
} metric_comment_t;

typedef enum {
    METRIC_SUB_TYPE_UNKNOWN,
    METRIC_SUB_TYPE_GAUGE,
    METRIC_SUB_TYPE_COUNTER_TOTAL,
    METRIC_SUB_TYPE_COUNTER_CREATED,
    METRIC_SUB_TYPE_STATE_SET,
    METRIC_SUB_TYPE_INFO,
    METRIC_SUB_TYPE_SUMMARY_COUNT,
    METRIC_SUB_TYPE_SUMMARY_SUM,
    METRIC_SUB_TYPE_SUMMARY_CREATED,
    METRIC_SUB_TYPE_SUMMARY,
    METRIC_SUB_TYPE_HISTOGRAM_COUNT,
    METRIC_SUB_TYPE_HISTOGRAM_SUM,
    METRIC_SUB_TYPE_HISTOGRAM_BUCKET,
    METRIC_SUB_TYPE_HISTOGRAM_CREATED,
    METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GCOUNT,
    METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GSUM,
    METRIC_SUB_TYPE_GAUGE_HISTOGRAM_BUCKET,
    METRIC_SUB_TYPE_GAUGE_HISTOGRAM_CREATED,
} metric_sub_type_t;

typedef struct metric_parser_s {
    char *metric_prefix;
    label_set_t labels;
    c_avl_tree_t *fams;
    size_t lineno;
    strbuf_t buf;
    metric_family_t *last_fam;
} metric_parser_t;

enum {
    SC_SPACE      = 1,
    SC_NEWLINE    = 2,
    SC_EQUAL      = 3,
    SC_LBRACE     = 4,
    SC_RBRACE     = 5,
    SC_COMMA      = 6,
    SC_COLON      = 7,
    SC_DIGIT      = 8,
    SC_ALPHA      = 9,
    SC_DQUOTE     = 10,
    SC_COMMENT    = 11,
    SC_UNEXPECTED = 12
};

static unsigned char scan_code[256] = {
     0, 12, 12, 12, 12, 12, 12, 12, 12,  1,  2, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
     1, 12, 10, 11, 12, 12, 12, 12, 12, 12, 12, 12,  6, 12, 12, 12,
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  7, 12, 12,  3, 12, 12,
    12,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9, 12, 12, 12, 12,  9,
    12,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  4, 12,  5, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
};

#if 0
static void scan_init(void) {
    int i;

    for (i = 0; i < 256; i++)
        scan_code[i] = SC_UNEXPECTED;
    for (i = '0'; i <= '9'; i++)
        scan_code[i] = SC_DIGIT;
    for (i = 'A'; i <= 'Z'; i++)
        scan_code[i] = SC_ALPHA;
    for (i = 'a'; i <= 'z'; i++)
        scan_code[i] = SC_ALPHA;

    scan_code[0] = 0;
    scan_code[' '] = scan_code['\t'] = SC_SPACE;
    scan_code['\n'] = SC_NEWLINE;
    scan_code['{'] = SC_LBRACE;
    scan_code['}'] = SC_RBRACE;
    scan_code['='] = SC_EQUAL;
    scan_code['#'] = SC_COMMENT;
    scan_code['\"'] = SC_DQUOTE;
    scan_code[','] = SC_COMMA;
    scan_code[':'] = SC_COLON;
    scan_code['_'] = SC_ALPHA;
}
#endif

static bool isinteger(const char *str)
{
    if ((str[0] == '-') || (str[0] == '+'))
        str++;

    while(*str != '\0') {
        if(!isdigit(*str))
            return false;
        str++;
    }

    return true;
}

static inline int sstrncmpend(const char *str, size_t size, const char *suffix, size_t suffix_size)
{
    if (suffix_size > size)
        return -1;
    return sstrncmp(str + (size - suffix_size), suffix_size, suffix, suffix_size);
}

static int metric_parser_fam_cmp(const void *a, const void *b)
{
    const metric_family_t *fam_a = a;
    const metric_family_t *fam_b = b;
    return strcmp(fam_a->name, fam_b->name);
}

static metric_family_t *metric_parser_get_family(metric_parser_t *mp, bool create,
                                                 const char *name, size_t name_size)
{
    char bname[4096];

    if (name_size > (sizeof(bname)-1)) {
        ERROR("family name greater than %zu.", sizeof(bname)-1);
        return NULL;
    }

    sstrnncpy(bname, sizeof(bname), name, name_size);

    metric_family_t qfam = { .name = bname };
    metric_family_t *fam = NULL;
    int status = c_avl_get(mp->fams, &qfam, (void *)&fam);
    if (status == 0) {
        return fam;
    }

    if (!create)
        return NULL;

    fam = calloc(1, sizeof (*fam));
    if (fam == NULL) {
        ERROR("calloc failed.");
        return NULL;
    }

    fam->type = METRIC_TYPE_UNKNOWN;

    fam->name = strdup(bname);
    if (fam->name == NULL) {
        free(fam);
        ERROR("strdup failed.");
        return NULL;
    }

    status = c_avl_insert(mp->fams, fam, (void *)fam);
    if (status != 0) {
        free(fam->name);
        free(fam);
        ERROR("c_avl_insert failed.");
        return NULL;
    }

    return fam;
}

static value_t *metric_familty_get_value(metric_family_t *fam, label_set_t *labels, cdtime_t time)
{
    if (fam->metric.num > 0) {
        metric_t *m = &fam->metric.ptr[fam->metric.num -1];
        if (label_set_cmp(labels, &m->label) == 0)
            return &m->value;
    }

    metric_t metric = (metric_t){.time = time};
    label_set_clone(&metric.label, *labels);
    int status = metric_list_append(&fam->metric, metric);
    if ((status == 0) && (fam->metric.num > 0)) {
        metric_t *m = &fam->metric.ptr[fam->metric.num -1];

        switch(fam->type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_INFO:
        case METRIC_TYPE_STATE_SET:
            break;
        case METRIC_TYPE_SUMMARY:
            m->value.summary = summary_new();
            if (m->value.summary == NULL)
                return NULL;
            break;
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            m->value.histogram = histogram_new();
            if (m->value.histogram == NULL)
                return NULL;
            break;
        }

        return &m->value;
    } else {
        label_set_reset(&metric.label); //FIXME
    }

    return NULL;
}

static ssize_t metric_parser_labels(label_set_t *labels, const char *line,
                                    const char *label_name, size_t label_name_size,
                                    const char **label_value, size_t *label_value_size)
{
    const char *ptr = line;
    unsigned char sc = 0;

    while (true) {
        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
        if (sc == SC_RBRACE)
            return ptr - line;

        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
        if (sc == 0)
            return -1;

        const char *label = ptr;
        if ((sc == SC_ALPHA) || (sc == SC_DIGIT)) {
            do {
                ptr++;
                sc = scan_code[(unsigned char)*ptr];
            } while ((sc == SC_ALPHA) || (sc == SC_DIGIT));
        } else {
            return -2;
        }

        size_t label_size = ptr - label;
        if (label_size == 0)
            return -3;

        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
        if (sc != SC_EQUAL)
            return -4;
        ptr++; // skip '='


        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
        if (sc != SC_DQUOTE)
            return -5;
        ptr++; // skip '"'


        const char *value = ptr;
        while (true) {
            if (*ptr == 0)
                return -1;
            if (*ptr == '"')
                break;
            if (*ptr == '\\') {
                ptr++;
                if (*ptr == 0)
                    return -6;
            }
            ptr++;
        }
        size_t value_size = ptr - value;
        if (*ptr != '"')
            return -7;
        ptr++;

        if ((label_name != NULL) && (label_name_size == label_size) &&
            (strncmp(label, label_name, label_size) == 0)) {
            *label_value = value;
            *label_value_size = value_size;
        } else {
            _label_set_add(labels, true, true, label, label_size, value, value_size);
        }

        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
        if (sc == SC_COMMA)
            ptr++;
    }

    return -8;
}

static size_t metric_parser_type_metric_len(metric_type_t type, const char *metric, size_t len)
{
    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        return len;
        break;
    case METRIC_TYPE_GAUGE:
        return len;
        break;
    case METRIC_TYPE_COUNTER:
        if (sstrncmpend(metric, len, "_total", strlen("_total")) == 0) {
            return len - strlen("_total");
        } else if (sstrncmpend(metric, len, "_created", strlen("_created")) == 0) {
            return len - strlen("_created");
        } else {
            return len;
        }
        break;
    case METRIC_TYPE_STATE_SET:
        return len;
        break;
    case METRIC_TYPE_INFO:
        if (sstrncmpend(metric, len, "_info", strlen("_info")) == 0) {
            return len - strlen("_info");
        } else {
            return len;
        }
        break;
    case METRIC_TYPE_SUMMARY:
        if (sstrncmpend(metric, len, "_count", strlen("_count")) == 0) {
            return len - strlen("_count");
        } else if (sstrncmpend(metric, len, "_sum", strlen("_sum")) == 0) {
            return len - strlen("_sum");
        } else if (sstrncmpend(metric, len, "_created", strlen("_created")) == 0) {
            return len - strlen("_created");
        } else {
            return len;
        }
        break;
    case METRIC_TYPE_HISTOGRAM:
        if (sstrncmpend(metric, len, "_count", strlen("_count")) == 0) {
            return len - strlen("_count");
        } else if (sstrncmpend(metric, len, "_sum", strlen("_sum")) == 0) {
            return len - strlen("_sum");
        } else if (sstrncmpend(metric, len, "_bucket", strlen("_bucket")) == 0) {
            return len - strlen("_bucket");
        } else if (sstrncmpend(metric, len, "_created", strlen("_created")) == 0) {
            return len - strlen("_created");
        } else {
            return len;
        }
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        if (sstrncmpend(metric, len, "_gcount", strlen("_gcount")) == 0) {
            return len - strlen("_gcount");
        } else if (sstrncmpend(metric, len, "_gsum", strlen("_gsum")) == 0) {
            return len - strlen("_gsum");
        } else if (sstrncmpend(metric, len, "_bucket", strlen("_bucket")) == 0) {
            return len - strlen("_bucket");
        } else if (sstrncmpend(metric, len, "_created", strlen("_created")) == 0) {
            return len - strlen("_created");
        } else {
            return len;
        }
        break;
    }

    return len;
}

static size_t metric_parser_guest_metric_len(const char *metric, size_t len)
{
    if (sstrncmpend(metric, len, "_total", strlen("_total")) == 0) {
        return len - strlen("_total");
    } else if (sstrncmpend(metric, len, "_created", strlen("_created")) == 0) {
        return len - strlen("_created");
    } else if (sstrncmpend(metric, len, "_info", strlen("_info")) == 0) {
        return len - strlen("_info");
    } else if (sstrncmpend(metric, len, "_count", strlen("_count")) == 0) {
        return len - strlen("_count");
    } else if (sstrncmpend(metric, len, "_sum", strlen("_sum")) == 0) {
        return len - strlen("_sum");
    } else if (sstrncmpend(metric, len, "_bucket", strlen("_bucket")) == 0) {
        return len - strlen("_bucket");
    } else if (sstrncmpend(metric, len, "_created", strlen("_created")) == 0) {
        return len - strlen("_created");
    } else if (sstrncmpend(metric, len, "_gcount", strlen("_gcount")) == 0) {
        return len - strlen("_gcount");
    } else if (sstrncmpend(metric, len, "_gsum", strlen("_gsum")) == 0) {
        return len - strlen("_gsum");
    }

    return len;
}

static metric_family_t *metric_parse_find_family(metric_parser_t *mp,
                                                 const char *metric, size_t metric_size)
{
    metric_family_t *fam = mp->last_fam;
    if (fam != NULL) {
        size_t metric_len = metric_parser_type_metric_len(fam->type, metric, metric_size);
        size_t name_len = strlen(fam->name);
        if (metric_len == name_len) {
            if (strncmp(metric, fam->name, name_len) == 0)
                return fam;
        }

        if (metric_size == name_len) {
            if (strncmp(metric, fam->name, name_len) == 0)
                return fam;
        }
    }

    mp->last_fam = NULL;

    size_t metric_len = metric_parser_guest_metric_len(metric, metric_size);
    if (metric_len != metric_size) {
        // maybe a suffixed metric
        //  XXX
    }

    fam = metric_parser_get_family(mp, false, metric, metric_len);
    if (fam != NULL) {
        mp->last_fam = fam;
        return fam;
    }

    fam = metric_parser_get_family(mp, true, metric, metric_size);
    if (fam != NULL) {
        mp->last_fam = fam;
        return fam;
    }
    return NULL;
}

static int metric_parse_metric(metric_parser_t *mp, const char *line)
{
    const char *ptr = line;
    unsigned char sc = 0;

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;
    if (sc == 0)
        return 0;

    const char *metric = ptr;
    if ((sc == SC_ALPHA) || (sc == SC_COLON)) {
        do {
            ptr++;
            sc = scan_code[(unsigned char)*ptr];
        } while ((sc == SC_ALPHA) || (sc == SC_COLON) || (sc == SC_DIGIT));
    } else {
        return -1; // FIXME __name__ ???
    }

    size_t metric_size = ptr - metric; // FIXME
    if (metric_size == 0)
        return -1;

    metric_family_t *fam = metric_parse_find_family(mp, metric, metric_size);
    if (fam == NULL) {// FIXME
        return -1;
    }

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;
    if (sc == 0)
        return -1;

    size_t metric_fixed_size = metric_size;
    metric_sub_type_t metric_sub_type = METRIC_SUB_TYPE_UNKNOWN;
    const char *label_name = NULL;
    size_t label_name_size = 0;

    switch(fam->type) {
    case METRIC_TYPE_UNKNOWN:
        metric_sub_type = METRIC_SUB_TYPE_UNKNOWN;
        break;
    case METRIC_TYPE_GAUGE:
        metric_sub_type = METRIC_SUB_TYPE_GAUGE;
        break;
    case METRIC_TYPE_COUNTER:
        if (sstrncmpend(metric, metric_size, "_total", strlen("_total")) == 0) {
            metric_fixed_size -= strlen("_total");
            metric_sub_type = METRIC_SUB_TYPE_COUNTER_TOTAL;
        } else if (sstrncmpend(metric, metric_size, "_created", strlen("_created")) == 0) {
            metric_fixed_size -= strlen("_created");
            metric_sub_type = METRIC_SUB_TYPE_COUNTER_CREATED;
        } else {
            metric_sub_type = METRIC_SUB_TYPE_COUNTER_TOTAL;
        }
        break;
    case METRIC_TYPE_STATE_SET:
        metric_sub_type = METRIC_SUB_TYPE_STATE_SET;
        label_name = metric;
        label_name_size = metric_size;
        break;
    case METRIC_TYPE_INFO:
        if (sstrncmpend(metric, metric_size, "_info", strlen("_info")) == 0) {
            metric_fixed_size -= strlen("_info");
            metric_sub_type = METRIC_SUB_TYPE_INFO;
        } else {
            metric_sub_type = METRIC_SUB_TYPE_INFO;
        }
        break;
    case METRIC_TYPE_SUMMARY:
        if (sstrncmpend(metric, metric_size, "_count", strlen("_count")) == 0) {
            metric_fixed_size -= strlen("_count");
            metric_sub_type = METRIC_SUB_TYPE_SUMMARY_COUNT;
        } else if (sstrncmpend(metric, metric_size, "_sum", strlen("_sum")) == 0) {
            metric_fixed_size -= strlen("_sum");
            metric_sub_type = METRIC_SUB_TYPE_SUMMARY_SUM;
        } else if (sstrncmpend(metric, metric_size, "_created", strlen("_created")) == 0) {
            metric_fixed_size -= strlen("_created");
            metric_sub_type = METRIC_SUB_TYPE_SUMMARY_CREATED;
        } else {
            metric_sub_type = METRIC_SUB_TYPE_SUMMARY;
            label_name = "quantile";
            label_name_size = strlen("quantile");
        }
        break;
    case METRIC_TYPE_HISTOGRAM:
        if (sstrncmpend(metric, metric_size, "_count", strlen("_count")) == 0) {
            metric_fixed_size -= strlen("_count");
            metric_sub_type = METRIC_SUB_TYPE_HISTOGRAM_COUNT;
        } else if (sstrncmpend(metric, metric_size, "_sum", strlen("_sum")) == 0) {
            metric_fixed_size -= strlen("_sum");
            metric_sub_type = METRIC_SUB_TYPE_HISTOGRAM_SUM;
        } else if (sstrncmpend(metric, metric_size, "_bucket", strlen("_bucket")) == 0) {
            metric_fixed_size -= strlen("_bucket");
            metric_sub_type = METRIC_SUB_TYPE_HISTOGRAM_BUCKET;
            label_name = "le";
            label_name_size = strlen("le");
        } else if (sstrncmpend(metric, metric_size, "_created", strlen("_created")) == 0) {
            metric_fixed_size -= strlen("_created");
            metric_sub_type = METRIC_SUB_TYPE_HISTOGRAM_CREATED;
        } else {
            return -1;
        }
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        if (sstrncmpend(metric, metric_size, "_gcount", strlen("_gcount")) == 0) {
            metric_fixed_size -= strlen("_gcount");
            metric_sub_type = METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GCOUNT;
        } else if (sstrncmpend(metric, metric_size, "_gsum", strlen("_gsum")) == 0) {
            metric_fixed_size -= strlen("_gsum");
            metric_sub_type = METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GSUM;
        } else if (sstrncmpend(metric, metric_size, "_bucket", strlen("_bucket")) == 0) {
            metric_fixed_size -= strlen("_bucket");
            metric_sub_type = METRIC_SUB_TYPE_GAUGE_HISTOGRAM_BUCKET;
            label_name = "le";
            label_name_size = strlen("le");
        } else if (sstrncmpend(metric, metric_size, "_created", strlen("_created")) == 0) {
            metric_fixed_size -= strlen("_created");
            metric_sub_type = METRIC_SUB_TYPE_GAUGE_HISTOGRAM_CREATED;
        } else {
            return -1;
        }
        break;
    }

    if (metric_sub_type == METRIC_SUB_TYPE_COUNTER_CREATED) {
        return 0;
    } else if (metric_sub_type == METRIC_SUB_TYPE_SUMMARY_CREATED) {
        return 0;
    } else if (metric_sub_type == METRIC_SUB_TYPE_HISTOGRAM_CREATED) {
        return 0;
    } else if (metric_sub_type == METRIC_SUB_TYPE_GAUGE_HISTOGRAM_CREATED) {
        return 0;
    }

    label_set_t labels = {0};
    const char *label_value = NULL;
    size_t label_value_size = 0;

    if (sc == SC_LBRACE) {
        ptr++;
        ssize_t size = metric_parser_labels(&labels, ptr, label_name, label_name_size,
                                            &label_value, &label_value_size);
        if (size < 0) {
            label_set_reset(&labels);
            return -1;
        }
        ptr += size;

        ptr++;

        sc = scan_code[(unsigned char)*ptr];
        if (sc != SC_SPACE) {
            label_set_reset(&labels);
            return -1;
        }
    }

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;
    if (sc == 0) {
        label_set_reset(&labels);
        return -1;
    }

    const char *value_number = ptr;
    do {
        ptr++;
        sc = scan_code[(unsigned char)*ptr];
    } while ((sc != SC_SPACE) && (sc != 0));

    size_t value_size = ptr - value_number;
    char number[64] = {'0','\0'};
    if (value_size > 0)
        sstrnncpy(number, sizeof(number), value_number, value_size);

    cdtime_t time = 0;
    if (sc == SC_SPACE) {
        while (scan_code[(unsigned char)*ptr] == SC_SPACE)
            ptr++;

        const char *timestamp = ptr;
        while (scan_code[(unsigned char)*ptr] == SC_DIGIT)
            ptr++;
        size_t timestamp_size = ptr - timestamp;
        if (timestamp_size > 0) {
            char buffer[16];
            sstrnncpy(buffer, sizeof(buffer), timestamp, timestamp_size);
            unsigned long long tms = strtoull(buffer, NULL, 10);
            time = MS_TO_CDTIME_T(tms);
        }
    }

    switch (metric_sub_type) {
    case METRIC_SUB_TYPE_UNKNOWN: {
        value_t value = VALUE_UNKNOWN(strtod(number, NULL));
        metric_t m = (metric_t){.value = value, .time = time};
        label_set_clone(&m.label, labels);
        metric_list_append(&fam->metric, m);
    }   break;
    case METRIC_SUB_TYPE_GAUGE: {
        value_t value = VALUE_GAUGE(strtod(number, NULL));
        metric_t m = (metric_t){.value = value, .time = time};
        label_set_clone(&m.label, labels);
        metric_list_append(&fam->metric, m);
    }   break;
    case METRIC_SUB_TYPE_COUNTER_TOTAL: {
        value_t value = {0};
        if(isinteger(number)) {
            value = VALUE_COUNTER(strtoull(number, NULL, 10));
        } else {
            value = VALUE_COUNTER_FLOAT64(strtod(number, NULL));
        }
        metric_t m = (metric_t){.value = value, .time = time};
        label_set_clone(&m.label, labels);
        metric_list_append(&fam->metric, m);
    }   break;
    case METRIC_SUB_TYPE_COUNTER_CREATED:
        break;
    case METRIC_SUB_TYPE_STATE_SET:
        if (label_value != NULL) {
            value_t *value = metric_familty_get_value(fam, &labels, time);
            if (value != NULL) {
                char bname[4096];
                sstrnncpy(bname, sizeof(bname), label_value, label_value_size);
                unsigned long long enabled = strtoull(number, NULL, 10);
                state_set_add(&(value->state_set), bname, enabled == 0 ? false : true);
            }
        }
        break;
    case METRIC_SUB_TYPE_INFO: {
        metric_t m = (metric_t){.time = time};
        label_set_clone(&m.label, labels);
        metric_list_append(&fam->metric, m);
    }   break;
    case METRIC_SUB_TYPE_SUMMARY_COUNT: {
        value_t *value = metric_familty_get_value(fam, &labels, time);
        if (value != NULL)
            value->summary->count = strtod(number, NULL);
    }   break;
    case METRIC_SUB_TYPE_SUMMARY_SUM: {
        value_t *value = metric_familty_get_value(fam, &labels, time);
        if (value != NULL)
            value->summary->sum = strtoull(number, NULL, 10);
    }   break;
    case METRIC_SUB_TYPE_SUMMARY_CREATED:
        break;
    case METRIC_SUB_TYPE_SUMMARY:
        if (label_value != NULL) {
            value_t *value = metric_familty_get_value(fam, &labels, time);
            if (value != NULL) {
                char vnumber[64] = {'0','\0'};
                sstrnncpy(vnumber, sizeof(vnumber), label_value, label_value_size);
                double quantile = strtod(vnumber, NULL);
                double qvalue = strtod(number, NULL);
                summary_t *summary = summary_quantile_append(value->summary, quantile, qvalue);
                if (summary != NULL)
                    value->summary = summary;
            }
        }
        break;
    case METRIC_SUB_TYPE_HISTOGRAM_COUNT:
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GCOUNT:
        break;
    case METRIC_SUB_TYPE_HISTOGRAM_SUM:
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GSUM: {
        value_t *value = metric_familty_get_value(fam, &labels, time);
        if (value != NULL)
            value->histogram->sum = strtod(number, NULL);
    }   break;
    case METRIC_SUB_TYPE_HISTOGRAM_BUCKET:
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_BUCKET:
        if (label_value != NULL) {
            value_t *value = metric_familty_get_value(fam, &labels, time);
            if (value != NULL) {
                char vnumber[64] = {'0','\0'};
                sstrnncpy(vnumber, sizeof(vnumber), label_value, label_value_size);
                double maximum = strtod(vnumber, NULL);
                uint64_t counter = strtoull(number, NULL, 10);
                histogram_t *histogram = histogram_bucket_append(value->histogram, maximum, counter);
                if (histogram != NULL)
                    value->histogram = histogram;
            }
        }
        break;
    case METRIC_SUB_TYPE_HISTOGRAM_CREATED:
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_CREATED:
        break;
    }

    label_set_reset(&labels);

    return 0;
}

static int metric_parse_type(const char *str, size_t len, metric_type_t *type)
{
    switch (len) {
    case 4:
        if (strncmp("info", str, len) == 0) {
            *type = METRIC_TYPE_INFO;
            return 0;
        }
        break;
    case 5:
        if (strncmp("gauge", str, len) == 0) {
            *type = METRIC_TYPE_GAUGE;
            return 0;
        }
        break;
    case 7:
        if (strncmp("counter", str, len) == 0) {
            *type = METRIC_TYPE_COUNTER;
            return 0;
        } else if (strncmp("unknown", str, len) == 0) {
            *type = METRIC_TYPE_UNKNOWN;
            return 0;
        } else if (strncmp("summary", str, len) == 0) {
            *type = METRIC_TYPE_SUMMARY;
            return 0;
        } else if (strncmp("untyped", str, len) == 0) {
            *type = METRIC_TYPE_UNKNOWN;
            return 0;
        }
        break;
    case 8:
        if (strncmp("stateset", str, len) == 0) {
            *type = METRIC_TYPE_STATE_SET;
            return 0;
        }
        break;
    case 9:
        if (strncmp("histogram", str, len) == 0) {
            *type = METRIC_TYPE_HISTOGRAM;
            return 0;
        }
        break;
    case 14:
        if (strncmp("gaugehistogram", str, len) == 0) {
            *type = METRIC_TYPE_GAUGE_HISTOGRAM;
            return 0;
        }
        break;
    }

    return -1;
}

int metric_parse_line(metric_parser_t *mp, const char *line)
{
    const char *ptr = line;
    unsigned char sc;

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;

    if (sc == 0)
        return 0;

    if ((sc == SC_COLON) || (sc == SC_ALPHA))
        return metric_parse_metric(mp, line);

    if (sc != SC_COMMENT)
        return -1;

    ptr++;

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;

    if (sc == 0)
        return 0;

    metric_comment_t comment_type;

    if (strncmp("HELP", ptr, strlen("HELP")) == 0) {
        comment_type = METRIC_COMMENT_HELP;
        ptr += strlen("HELP");
    } else if (strncmp("TYPE", ptr, strlen("TYPE")) == 0) {
        comment_type = METRIC_COMMENT_TYPE;
        ptr += strlen("TYPE");
    } else if (strncmp("UNIT", ptr, strlen("UNIT")) == 0) {
        comment_type = METRIC_COMMENT_UNIT;
        ptr += strlen("UNIT");
    } else if (strncmp("END", ptr, strlen("END")) == 0) {
        comment_type = METRIC_COMMENT_END;
        ptr += strlen("END");
    } else if (strncmp("EOF", ptr, strlen("EOF")) == 0) {
        comment_type = METRIC_COMMENT_END;
        ptr += strlen("EOF");
    } else {
        return 0;
    }

    if (comment_type == METRIC_COMMENT_END) {
        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
        if (sc == 0)
            return 1;
        return 0;
    }

    if (scan_code[(unsigned char)*ptr] == SC_SPACE) {
        while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
            ptr++;
    } else {
        return 0;
    }

    const char *metric = ptr;
    if ((sc == SC_ALPHA) || (sc == SC_COLON)) {
        do {
            ptr++;
            sc = scan_code[(unsigned char)*ptr];
        } while ((sc == SC_ALPHA) || (sc == SC_COLON) || (sc == SC_DIGIT));
    } else {
        return -1;
    }
    size_t metric_size = ptr - metric; // FIXME
    if (metric_size == 0)
        return -1;

    metric_family_t *fam = metric_parser_get_family(mp, true, metric, metric_size);
    if (fam == NULL) {
        //  FIXME
        return -1;
    }
    mp->last_fam = fam;

    if (sc == SC_SPACE) {
        while (scan_code[(unsigned char)*ptr] == SC_SPACE)
            ptr++;
    } else {
        return -1;
    }

    const char *text = ptr;
    while (scan_code[(unsigned char)*ptr] != 0)
        ptr++;
    size_t text_size = ptr - text;
    if (text_size == 0)
        return -1;

    switch (comment_type) {
    case METRIC_COMMENT_HELP:
        if (fam->help != NULL) // FIXME
            free(fam->help);
        fam->help = sstrndup(text, text_size);
        if (fam->help == NULL)
            return -1;
        break;
    case METRIC_COMMENT_TYPE: {
        metric_type_t type = 0;
        if (metric_parse_type(text, text_size, &type) != 0)
            return -1;
        if (fam->metric.num != 0) {
            if (fam->type != type) {
                return -1; // FIXME
            }
        } else {
            fam->type = type;
        }
    }    break;
    case METRIC_COMMENT_UNIT:
        if (fam->unit != NULL) // FIXME
            free(fam->unit);
        fam->unit = sstrndup(text, text_size);
        if (fam->unit == NULL)
            return -1;
        break;
    default:
        break;
    }

    return 0;
}

int metric_parse_buffer(metric_parser_t *mp, char *buffer, size_t buffer_len)
{
    if (buffer == NULL) {
        if (strbuf_len(&mp->buf) > 0) {
            int status = metric_parse_line(mp, mp->buf.ptr);
            if (status < 0)
                return status;
        }
        return 0;
    }

    while (buffer_len > 0) {
        char *end = memchr(buffer, '\n', buffer_len);
        if (end != NULL) {
            size_t line_size = end - buffer;
            mp->lineno += 1;
            if (line_size > 0) { // FIXME
                strbuf_putstrn(&mp->buf, buffer, line_size);

                int status = metric_parse_line(mp, mp->buf.ptr);
                if (status < 0) {
                    return status;
                }
            }

            strbuf_reset(&mp->buf);
            buffer_len -= line_size; // FIXME +1
            buffer = end + 1;              // +1
        } else {
            strbuf_putstrn(&mp->buf, buffer, buffer_len);
            buffer_len = 0;
        }
    }

    return 0;
}

void metric_parser_reset(metric_parser_t *mp)
{
    while (true) {
        metric_family_t *fam;
        int status = c_avl_pick(mp->fams, (void *)&fam,  NULL);
        if (status != 0)
            break;
        metric_family_free(fam);
    }

    mp->lineno = 0;
    mp->last_fam = NULL;
    strbuf_reset(&mp->buf);
}

void metric_parser_free(metric_parser_t *mp)
{
    if (mp == NULL)
        return;

    metric_parser_reset(mp);

    free(mp->metric_prefix);
    label_set_reset(&mp->labels);

    c_avl_destroy(mp->fams);

    strbuf_destroy(&mp->buf);

    free(mp);
}

metric_parser_t *metric_parser_alloc(char *metric_prefix, label_set_t *labels)
{
    metric_parser_t *mp = calloc(1, sizeof(*mp));
    if (mp == NULL)
        return NULL;

    if (metric_prefix != NULL) {
        mp->metric_prefix = strdup(metric_prefix);
        if (mp->metric_prefix == NULL) {
            ERROR("strdup failed");
            metric_parser_free(mp);
            return NULL;
        }
    }

    if ((labels != NULL) && (labels->num > 0))
        label_set_clone(&mp->labels, *labels);

    mp->fams = c_avl_create(metric_parser_fam_cmp);
    if (mp->fams == NULL) {
        metric_parser_free(mp);
        return NULL;
    }

    mp->buf = STRBUF_CREATE;

    return mp;
}

int metric_parser_dispatch(metric_parser_t *mp, dispatch_metric_family_t dispatch,
                                                plugin_filter_t *filter, cdtime_t time)
{
    size_t metric_prefix_size = 0;

    if (mp->metric_prefix != NULL)
        metric_prefix_size = strlen(mp->metric_prefix);

    if (time == 0)
        time = cdtime();

    while (true) {
        metric_family_t *fam;
        int status = c_avl_pick(mp->fams, (void *)&fam,  NULL);
        if (status != 0)
            break;

        if (mp->metric_prefix != NULL) {
            size_t len = strlen(fam->name);
            char *name = malloc(len + metric_prefix_size + 1);
            if (name == NULL) {
                ERROR("malloc failed.");
                metric_family_free(fam);
                continue;
            }
            memcpy(name, fam->name, len);
            memcpy(name+len, mp->metric_prefix, metric_prefix_size);
            name[len + metric_prefix_size] = '\0';
            free(fam->name);
            fam->name = name;
        }

        if (fam->type == METRIC_TYPE_COUNTER) {
            size_t len = strlen(fam->name);
            if (sstrncmpend(fam->name, len, "_total", strlen("_total")) == 0)
                fam->name[len - strlen("_total")] = '\0';
        } else if (fam->type == METRIC_TYPE_INFO) {
            size_t len = strlen(fam->name);
            if (sstrncmpend(fam->name, len, "_info", strlen("_info")) == 0)
                fam->name[len - strlen("_info")] = '\0';
        }

        if (mp->labels.num > 0) {
            for (size_t i = 0; i < fam->metric.num; i++) {
                metric_t *m = &fam->metric.ptr[i];
                label_set_add_set(&m->label, true, mp->labels);
            }
        }

        dispatch(fam, filter, time);

        metric_family_free(fam);
    }

    return 0;
}

int metric_parser_size(metric_parser_t *mp)
{
    if (mp == NULL)
        return 0;
    return c_avl_size(mp->fams);
}

