#include "collectd.h"

#include "utils/format_openmetrics/format_openmetrics.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils_cache.h"

int format_openmetrics_metric_family(strbuf_t *buf, metric_family_t const *fam)
{
  if ((buf == NULL) || (fam == NULL))
    return EINVAL;

  if (fam->metric.num == 0)
    return 0;

  char *type = NULL;

  switch (fam->type) {
    case METRIC_TYPE_GAUGE:
      type = "gauge";
     break;
    case METRIC_TYPE_COUNTER:
      type = "counter";
      break;
    case METRIC_TYPE_UNTYPED:
      type = "untyped";
      break;
    case METRIC_TYPE_DISTRIBUTION: // FIXME
      break;
  }

  if (type == NULL)
    return EINVAL;

  int status = 0;
  if (fam->help == NULL)
    status = status | strbuf_printf(buf, "# HELP %s\n", fam->name);
  else
    status = status | strbuf_printf(buf, "# HELP %s %s\n", fam->name, fam->help);
  status = status | strbuf_printf(buf, "# TYPE %s %s\n", fam->name, type);

  for (size_t i = 0; i < fam->metric.num; i++) {
    metric_t *m = &fam->metric.ptr[i];

    status = status | metric_identity(buf, m);

    if (fam->type == METRIC_TYPE_COUNTER)
      status = status | strbuf_printf(buf, " %" PRIu64, m->value.counter);
    else
      status = status | strbuf_printf(buf, " " GAUGE_FORMAT, m->value.gauge);

    if (m->time > 0) {
      status = status | strbuf_printf(buf, " %" PRIi64 "\n", CDTIME_T_TO_MS(m->time));
    } else {
      status = status | strbuf_printf(buf, "\n");
    }
  }

  return status;
} 
