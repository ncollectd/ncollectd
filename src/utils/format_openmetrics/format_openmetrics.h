#ifndef UTILS_FORMAT_OPENMETRICS_H
#define UTILS_FORMAT_OPENMETRICS_H 1

#include "collectd.h"

#include "plugin.h"
#include "utils/strbuf/strbuf.h"

int format_openmetrics_metric_family(strbuf_t *buf, metric_family_t const *fam);

#endif /* UTILS_FORMAT_OPENMETRICS_H */
