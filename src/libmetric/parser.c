// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "log.h"
#include "libutils/strbuf.h"
#include "libutils/common.h"
#include "libmetric/metric.h"
#include "libmetric/metric_chars.h"

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

int metric_parser_dispatch(metric_family_t *fam, dispatch_metric_family_t dispatch,
                           plugin_filter_t *filter)
{
    if ((fam->metric.num > 0) && (dispatch != NULL))
        dispatch(fam, filter, 0);

    if (fam->name != NULL) {
        free(fam->name);
        fam->name = NULL;
    }

    if (fam->help != NULL) {
        free(fam->help);
        fam->help = NULL;
    }

    if (fam->unit != NULL) {
        free(fam->unit);
        fam->unit = NULL;
    }

    // FIXME clean metrics

    return 0;
}

static value_t *metric_familty_get_metric_value(metric_family_t *fam, label_set_t *labels,
                                                                      cdtime_t time)
{
    if (fam->metric.num > 0) {
        metric_t *m = &fam->metric.ptr[fam->metric.num -1];
        if (label_set_cmp(labels, &m->label) == 0)
            return &m->value;
    }

    int status = metric_list_append(&fam->metric, (metric_t){.label = *labels, .time = time});
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
    }

    return NULL;
}
static int metric_parser_set_family_name(metric_family_t *fam,
                                         const char *prefix, size_t prefix_size,
                                         const char *name, size_t name_size)
{
    if (fam->name != NULL)
        free(fam->name);

    size_t fam_name_size = prefix_size + name_size;
    fam->name = malloc(fam_name_size + 1);
    if (fam->name == NULL)
        return -1;
    if (prefix_size > 0)
        memcpy(fam->name, prefix, prefix_size);
    memcpy(fam->name + prefix_size, name, name_size);
    fam->name[fam_name_size] = '\0';

    return 0;
}

static int metric_parser_push(metric_family_t *fam, dispatch_metric_family_t dispatch,
                              plugin_filter_t *filter, const char *prefix, size_t prefix_size,
                              const char *name, size_t name_size)
{
    size_t fixed_name_size = name_size;

    if (sstrncmpend(name, name_size, "_total", strlen("_total")) == 0) {
        fixed_name_size -= strlen("_total");
    } else if ( sstrncmpend(name, name_size, "_info", strlen("_info")) == 0) {
        fixed_name_size -= strlen("_info");
    }

    if (fam->name == NULL)
        return metric_parser_set_family_name(fam, prefix, prefix_size, name, fixed_name_size);

   if ((strlen(fam->name) - prefix_size == fixed_name_size) &&
       (strncmp(fam->name + prefix_size, name, fixed_name_size) == 0))
       return 0;

    metric_parser_dispatch(fam, dispatch, filter);

    return metric_parser_set_family_name(fam, prefix, prefix_size, name, fixed_name_size);
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

static int metric_parse_metric(metric_family_t *fam, dispatch_metric_family_t dispatch,
                               plugin_filter_t *filter, const char *prefix, size_t prefix_size,
                               label_set_t *labels_extra, cdtime_t interval, cdtime_t ts,
                               const char *line)
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
            return -1;
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
            return -1;
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

    if (fam->name != NULL) {
        if ((strlen(fam->name) - prefix_size) != metric_fixed_size) {
            metric_parser_dispatch(fam, dispatch, filter);
        } else if (strncmp(fam->name + prefix_size, metric, metric_fixed_size) != 0) {
            metric_parser_dispatch(fam, dispatch, filter);
        }
    }

    if (fam->name == NULL)
        metric_parser_set_family_name(fam, prefix, prefix_size, metric, metric_fixed_size);

    label_set_t labels = {0};
    const char *label_value = NULL;
    size_t label_value_size = 0;

    if (sc == SC_LBRACE) {
        ptr++;
        ssize_t size = metric_parser_labels(&labels, ptr, label_name, label_name_size,
                                            &label_value, &label_value_size);
        if (size < 0) {
            // label free
            return -1;
        }
        ptr += size;

        ptr++;

        sc = scan_code[(unsigned char)*ptr];
        if (sc != SC_SPACE) {
        // label free
            return -1;
        }
    }

    // FIX add extra labels
    if (labels_extra != NULL)
        label_set_add_set(&labels, true, *labels_extra);

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;
    if (sc == 0) {
        // label free
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
    if (ts != 0) {
        time = ts;
    } else {
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
    }

    value_t value = {0};
    switch (metric_sub_type) {
    case METRIC_SUB_TYPE_UNKNOWN:
        value = VALUE_UNKNOWN(strtod(number, NULL));
        metric_list_append(&fam->metric, (metric_t){.label = labels, .value = value, .interval = interval, .time = time});
        break;
    case METRIC_SUB_TYPE_GAUGE:
        value = VALUE_GAUGE(strtod(number, NULL));
        metric_list_append(&fam->metric, (metric_t){.label = labels, .value = value, .interval = interval, .time = time});
        break;
    case METRIC_SUB_TYPE_COUNTER_TOTAL:
        if(isinteger(number)) {
            value = VALUE_COUNTER(strtoull(number, NULL, 10));
        } else {
            value = VALUE_COUNTER_FLOAT64(strtod(number, NULL));
        }
        metric_list_append(&fam->metric, (metric_t){.label = labels, .value = value, .interval = interval, .time = time});
        break;
    case METRIC_SUB_TYPE_COUNTER_CREATED:
        break;
    case METRIC_SUB_TYPE_STATE_SET: {
        value_t *fvalue = metric_familty_get_metric_value(fam, &labels, time);
        if (fvalue != NULL) {
//        int state_set_add(state_set_t *set, char const *name, bool enabled);
        }
//        value.state_set
//    state_set_t state_set;
//        int state_set_add(state_set_t *set, char const *name, bool enabled);
    }   break;
    case METRIC_SUB_TYPE_INFO:
        metric_list_append(&fam->metric, (metric_t){.label = labels, .value = value, .interval = interval, .time = time});
        break;
    case METRIC_SUB_TYPE_SUMMARY_COUNT:
        // uint64_t
//        value.summary->count =
//    state_set_t state_set;
//    label_set_t info;
//    histogram_t *histogram;
//    summary_t *summary;
        break;
    case METRIC_SUB_TYPE_SUMMARY_SUM:
//        value.summary->sum =
        break;
    case METRIC_SUB_TYPE_SUMMARY_CREATED:
        break;
    case METRIC_SUB_TYPE_SUMMARY: {
//        value_t *svalue = metric_parse_get_value(fam, labels, time);
//        if (svalue == NULL)
//            return -1;
//        summary_t *tmp = summary_quantile_append(svalue->summary, double quantile, double value);
    }   break;
    case METRIC_SUB_TYPE_HISTOGRAM_COUNT:
//        value.histogram->count
        break;
    case METRIC_SUB_TYPE_HISTOGRAM_SUM:
//        value.histogram->sum
        break;
    case METRIC_SUB_TYPE_HISTOGRAM_BUCKET:
//        histogram_t *histogram_bucket_append(histogram_t *h, double maximum, uint64_t counter);
        break;
    case METRIC_SUB_TYPE_HISTOGRAM_CREATED:
        break;
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GCOUNT:
//        value.histogram->count
        break;
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_GSUM:
//        value.histogram->sum
        break;
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_BUCKET:
        break;
    case METRIC_SUB_TYPE_GAUGE_HISTOGRAM_CREATED:
        break;
    }

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

int metric_parse_line(metric_family_t *fam, dispatch_metric_family_t dispatch,
                      plugin_filter_t *filter, const char *prefix, size_t prefix_size,
                      label_set_t *labels_extra, cdtime_t interval, cdtime_t ts, const char *line)
{
    const char *ptr = line;
    unsigned char sc;

    while ((sc = scan_code[(unsigned char)*ptr]) == SC_SPACE)
        ptr++;

    if (sc == 0)
        return 0;

    if ((sc == SC_COLON) || (sc == SC_ALPHA))
        return metric_parse_metric(fam, dispatch, filter, prefix, prefix_size,
                                   labels_extra, interval, ts, line);

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
        if (sc == 0) {
            metric_parser_dispatch(fam, dispatch, filter);
            return 1;
        }
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

    metric_parser_push(fam, dispatch, filter, prefix, prefix_size, metric, metric_size);

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
        if (fam->help != NULL)
            free(fam->help);
        fam->help = sstrndup(text, text_size);
        if (fam->help == NULL)
            return -1;
        break;
    case METRIC_COMMENT_TYPE:
        if (fam->metric.num != 0)
            return -1;
        if (metric_parse_type(text, text_size, &fam->type) != 0)
            return -1;
        break;
    case METRIC_COMMENT_UNIT:
        if (fam->unit != NULL)
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

/* parse_label_value reads a label value, unescapes it and prints it to buf. On
 * success, inout is updated to point to the character just *after* the label
 * value, i.e. the character *following* the ending quotes - either a comma or
 * closing curlies. */
static int parse_label_value(strbuf_t *buf, char const **inout)
{
    char const *ptr = *inout;

    if (ptr[0] != '"')
        return EINVAL;
    ptr++;

    while (ptr[0] != '"') {
        size_t valid_len = strcspn(ptr, "\\\"\n");
        if (valid_len != 0) {
            strbuf_putstrn(buf, ptr, valid_len);
            ptr += valid_len;
            continue;
        }

        if ((ptr[0] == 0) || (ptr[0] == '\n')) {
            return EINVAL;
        }

        assert(ptr[0] == '\\');
        if (ptr[1] == 0)
            return EINVAL;

        char tmp;
        if (ptr[1] == 'n') {
            tmp = '\n';
        } else if (ptr[1] == 'r') {
            tmp = '\r';
        } else if (ptr[1] == 't') {
            tmp = '\t';
        } else {
            tmp = ptr[1];
        }

        strbuf_putchar(buf, tmp);

        ptr += 2;
    }

    assert(ptr[0] == '"');
    ptr++;
    *inout = ptr;
    return 0;
}

int label_set_unmarshal(label_set_t *labels, char const **inout)
{
    int ret = 0;
    char const *ptr = *inout;

    if (ptr[0] != '{')
        return EINVAL;

    strbuf_t value = STRBUF_CREATE;
    while ((ptr[0] == '{') || (ptr[0] == ',')) {
        ptr++;

        size_t key_len = label_valid_name_len(ptr);
        if (key_len == 0) {
            ret = EINVAL;
            break;
        }
        char key[key_len + 1];
        strncpy(key, ptr, key_len);
        key[key_len] = 0;
        ptr += key_len;

        if (ptr[0] != '=') {
            ret = EINVAL;
            break;
        }
        ptr++;

        strbuf_reset(&value);
        int status = parse_label_value(&value, &ptr);
        if (status != 0) {
            ret = status;
            break;
        }

        status = label_set_add(labels, true, key, value.ptr);
        if (status != 0) {
            ret = status;
            break;
        }
    }
    strbuf_destroy(&value);

    if (ret != 0)
        return ret;

    if (ptr[0] != '}')
        return EINVAL;

    *inout = &ptr[1];

    return 0;
}
