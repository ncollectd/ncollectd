/**
 * collectd - src/threshold.c
 * Copyright (C) 2007-2010  Florian Forster
 * Copyright (C) 2008-2009  Sebastian Harl
 * Copyright (C) 2009       Andrés J. Díaz
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Author:
 *   Florian octo Forster <octo at collectd.org>
 *   Sebastian Harl <sh at tokkee.org>
 *   Andrés J. Díaz <ajdiaz at connectical.com>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/avltree/avltree.h"
#include "utils/common/common.h"
#include "utils_cache.h"
#include "utils_threshold.h"

/*
 * Threshold management
 * ====================
 * The following functions add, delete, search, etc. configured thresholds to
 * the underlying AVL trees.
 */

static void ut_threshold_free(threshold_t *th) {
  if (th == NULL)
    return;
  sfree(th->name);
  label_set_reset(&th->labels);
  sfree(th);
}

/*
 * int ut_threshold_add
 *
 * Adds a threshold configuration to the list of thresholds. The threshold_t
 * structure is copied and may be destroyed after this call. Returns zero on
 * success, non-zero otherwise.
 */
static int ut_threshold_add(threshold_t *th) { /* {{{ */
  char *name_copy = strdup(th->name);
  if (name_copy == NULL) {
    ERROR("ut_threshold_add: strdup failed.");
    return -1;
  }

  DEBUG("ut_threshold_add: Adding entry `%s'", name_copy);

  threshold_t *th_ptr = threshold_get(th->name);

  while ((th_ptr != NULL) && (th_ptr->next != NULL))
    th_ptr = th_ptr->next;

  int status = 0;
  if (th_ptr == NULL) /* no such threshold yet */
  {
    status = c_avl_insert(threshold_tree, name_copy, th);
  } else /* th_ptr points to the last threshold in the list */
  {
    th_ptr->next = th;
    /* name_copy isn't needed */
    sfree(name_copy);
  }

  if (status != 0) {
    ERROR("ut_threshold_add: c_avl_insert (%s) failed.", th->name);
    sfree(name_copy);
  }

  return status;
} /* }}} int ut_threshold_add */

/*
 * Configuration
 * =============
 * {{{ */
static int ut_config_metric(oconfig_item_t *ci) {
  if ((ci->values_num != 1) || (ci->values[0].type != OCONFIG_TYPE_STRING)) {
    WARNING("threshold values: The `Metric' block needs exactly one string "
            "argument.");
    return -1;
  }

  if (ci->children_num < 1) {
    WARNING("threshold values: The `Metric' block needs at least one option.");
    return -1;
  }

  threshold_t *th = calloc(1, sizeof(*th));
  if (th == NULL) {
    ERROR("threshold: ut_config_metric: calloc failed.");
    return -1;
  }

  th->name = strdup(ci->values[0].value.string);
  if (th->name == NULL) {
    ERROR("threshold: ut_config_metric: strdup failed.");
    return -1;
  }

  th->warning_min = NAN;
  th->warning_max = NAN;
  th->failure_min = NAN;
  th->failure_max = NAN;
  th->hits = 0;
  th->hysteresis = 0;
  th->flags = UT_FLAG_INTERESTING; /* interesting by default */

  int status = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *option = ci->children + i;

    if (strcasecmp("Label", option->key) == 0)
      status = cf_util_get_label(option, &th->labels);
    else if (strcasecmp("WarningMax", option->key) == 0)
      status = cf_util_get_double(option, &th->warning_max);
    else if (strcasecmp("FailureMax", option->key) == 0)
      status = cf_util_get_double(option, &th->failure_max);
    else if (strcasecmp("WarningMin", option->key) == 0)
      status = cf_util_get_double(option, &th->warning_min);
    else if (strcasecmp("FailureMin", option->key) == 0)
      status = cf_util_get_double(option, &th->failure_min);
    else if (strcasecmp("Interesting", option->key) == 0)
      status = cf_util_get_flag(option, &th->flags, UT_FLAG_INTERESTING);
    else if (strcasecmp("Invert", option->key) == 0)
      status = cf_util_get_flag(option, &th->flags, UT_FLAG_INVERT);
    else if (strcasecmp("Persist", option->key) == 0)
      status = cf_util_get_flag(option, &th->flags, UT_FLAG_PERSIST);
    else if (strcasecmp("PersistOK", option->key) == 0)
      status = cf_util_get_flag(option, &th->flags, UT_FLAG_PERSIST_OK);
    else if (strcasecmp("Hits", option->key) == 0)
      status = cf_util_get_int(option, &th->hits);
    else if (strcasecmp("Hysteresis", option->key) == 0)
      status = cf_util_get_double(option, &th->hysteresis);
    else {
      WARNING("threshold values: Option `%s' not allowed inside a `Type' "
              "block.",
              option->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  if (status == 0)
     status = ut_threshold_add(th);

  if (status != 0)
    ut_threshold_free(th);

  return status;
} /* int ut_config_metric */


/*
 * int ut_report_state
 *
 * Checks if the `state' differs from the old state and creates a notification
 * if appropriate.
 * Does not fail.
 */
static int ut_report_state(metric_t const *m, const char *name, const threshold_t *th, const gauge_t value, int state) { /* {{{ */
  /* Check if hits matched */
  if ((th->hits != 0)) {
    int hits = uc_get_hits_by_name(name);
    /* STATE_OKAY resets hits unless PERSIST_OK flag is set. Hits resets if
     * threshold is hit. */
    if (((state == STATE_OKAY) && ((th->flags & UT_FLAG_PERSIST_OK) == 0)) ||
        (hits > th->hits)) {
      DEBUG("ut_report_state: reset uc_get_hits = 0");
      uc_set_hits_by_name(name, 0); /* reset hit counter and notify */
    } else {
      DEBUG("ut_report_state: th->hits = %d, uc_get_hits = %d", th->hits,
            uc_get_hits_by_name(name));
      (void)uc_inc_hits_by_name(name, 1); /* increase hit counter */
      return 0;
    }
  } /* end check hits */

  int state_old = uc_get_state_by_name(name);

  /* If the state didn't change, report if `persistent' is specified. If the
   * state is `okay', then only report if `persist_ok` flag is set. */
  if (state == state_old) {
    if (state == STATE_UNKNOWN) {
      /* From UNKNOWN to UNKNOWN. Persist doesn't apply here. */
      return 0;
    } else if ((th->flags & UT_FLAG_PERSIST) == 0)
      return 0;
    else if ((state == STATE_OKAY) && ((th->flags & UT_FLAG_PERSIST_OK) == 0))
      return 0;
  }

  if (state != state_old)
    uc_set_state_by_name(name, state);

  notification_t n = {0};
  notification_init_metric(&n, NOTIF_FAILURE, m);

  if (state == STATE_OKAY)
    n.severity = NOTIF_OKAY;
  else if (state == STATE_WARNING)
    n.severity = NOTIF_WARNING;
  else
    n.severity = NOTIF_FAILURE;

  n.time = m->time;


  char number[64] = "";

  snprintf(number, sizeof(number), GAUGE_FORMAT, value);
  notification_annotation_set(&n, "current_value", number);

  snprintf(number, sizeof(number), GAUGE_FORMAT, th->warning_min);
  notification_annotation_set(&n, "warning_min", number);

  snprintf(number, sizeof(number), GAUGE_FORMAT, th->warning_max);
  notification_annotation_set(&n, "warning_max", number);

  snprintf(number, sizeof(number), GAUGE_FORMAT, th->failure_min);
  notification_annotation_set(&n, "failure_min", number);

  snprintf(number, sizeof(number), GAUGE_FORMAT, th->failure_max);
  notification_annotation_set(&n, "failure_max", number);

  strbuf_t buf = STRBUF_CREATE;
  strbuf_print(&buf, name);

  /* Send an okay notification */
  if (state == STATE_OKAY) {
    if (state_old == STATE_MISSING)
      strbuf_printf(&buf, ": Value is no longer missing.");
    else
      strbuf_printf(&buf, ": All data sources are within range again. Current value is %f.", value);
  } else if (state == STATE_UNKNOWN) {
    ERROR("ut_report_state: metric transition to UNKNOWN from a different "
          "state. This shouldn't happen.");
    STRBUF_DESTROY(buf);
    return 0;
  } else {
    double min;
    double max;

    min = (state == STATE_ERROR) ? th->failure_min : th->warning_min;
    max = (state == STATE_ERROR) ? th->failure_max : th->warning_max;

    if (th->flags & UT_FLAG_INVERT) {
      if (!isnan(min) && !isnan(max)) {
        strbuf_printf(&buf, ": Current value is %f. That is within the %s region of %f and %f.",
                  value, (state == STATE_ERROR) ? "failure" : "warning", min, max);
      } else {
        strbuf_printf(&buf, ": Current value is %f. That is %s the %s threshold of %f.",
                  value, isnan(min) ? "below" : "above",
                  (state == STATE_ERROR) ? "failure" : "warning",
                  isnan(min) ? max : min);
      }
    } else /* is not inverted */
    {
      strbuf_printf(&buf, ": Current value is %f. That is %s the %s threshold of %f.",
                value, (value < min) ? "below" : "above",
                (state == STATE_ERROR) ? "failure" : "warning",
                (value < min) ? min : max);
    }
  }

  notification_annotation_set(&n, "summary", buf.ptr);
  STRBUF_DESTROY(buf);

  plugin_dispatch_notification(&n);
  notification_reset(&n);
  return 0;
} /* }}} int ut_report_state */

/*
 * int ut_check_one_threshold
 *
 * Checks a value against the given threshold configuration. If the
 * `DataSource' option is set in the threshold, and the name does NOT match,
 * `okay' is returned. If the threshold does match, its failure and warning
 * min and max values are checked and `failure' or `warning' is returned if
 * appropriate.
 * Does not fail.
 *
 *
 * Checks value against the given threshold, using
 * the ut_check_one_data_source function above. Returns the worst status,
 * which is `okay' if nothing has failed or `unknown' if no valid datasource was
 * defined.
 * Returns less than zero if the data set doesn't have any data sources.
 */
static int ut_check_one_threshold(metric_t const *m, const char *name,
                                  const threshold_t *th,
                                  const gauge_t value) { /* {{{ */
  int is_warning = 0;
  int is_failure = 0;
  int prev_state = STATE_OKAY;

  if ((th->flags & UT_FLAG_INVERT) != 0) {
    is_warning--;
    is_failure--;
  }

  if (th->hysteresis > 0) {
    prev_state = uc_get_state_by_name(name);
    /* The purpose of hysteresis is elliminating flapping state when the value
     * oscilates around the thresholds. In other words, what is important is
     * the previous state; if the new value would trigger a transition, make
     * sure that we artificially widen the range which is considered to apply
     * for the previous state, and only trigger the notification if the value
     * is outside of this expanded range.
     *
     * There is no hysteresis for the OKAY state.
     * */
    gauge_t hysteresis_for_warning = 0, hysteresis_for_failure = 0;
    switch (prev_state) {
    case STATE_ERROR:
      hysteresis_for_failure = th->hysteresis;
      break;
    case STATE_WARNING:
      hysteresis_for_warning = th->hysteresis;
      break;
    case STATE_UNKNOWN:
    case STATE_OKAY:
      /* do nothing -- the hysteresis only applies to the non-normal states */
      break;
    }

    if ((!isnan(th->failure_min) &&
         (th->failure_min + hysteresis_for_failure > value)) ||
        (!isnan(th->failure_max) &&
         (th->failure_max - hysteresis_for_failure < value)))
      is_failure++;

    if ((!isnan(th->warning_min) &&
         (th->warning_min + hysteresis_for_warning > value)) ||
        (!isnan(th->warning_max) &&
         (th->warning_max - hysteresis_for_warning < value)))
      is_warning++;

  } else { /* no hysteresis */
    if ((!isnan(th->failure_min) && (th->failure_min > value)) ||
        (!isnan(th->failure_max) && (th->failure_max < value)))
      is_failure++;

    if ((!isnan(th->warning_min) && (th->warning_min > value)) ||
        (!isnan(th->warning_max) && (th->warning_max < value)))
      is_warning++;
  }

  if (is_failure != 0)
    return STATE_ERROR;

  if (is_warning != 0)
    return STATE_WARNING;

  return STATE_OKAY;
} /* }}} int ut_check_one_threshold */

static int ut_check_metric_threshold(metric_t const *m, threshold_t *th)
{
  strbuf_t buf = STRBUF_CREATE;
  int status = metric_identity(&buf, m);
  if (status != 0) {
    STRBUF_DESTROY(buf);
    return status;
  }
  const char *name = buf.ptr;

  gauge_t value;
  status = uc_get_rate_by_name(name, &value);
  if (status != 0) {
    STRBUF_DESTROY(buf);
    return 0;
  }

  int worst_state = -1;
  threshold_t *worst_th = NULL;
  int nmatches = 0;
  while (th != NULL) {
    bool match = true;

    for (size_t i = 0; i < th->labels.num; i++) {
      char const *value = metric_label_get(m, th->labels.ptr[i].name);
      if (value == NULL) {
        match = false;
        break;
      }
      if (strcmp(th->labels.ptr[i].value, value) !=0) {
        match = false;
        break;
      }
    }

    if (match) {
      status = ut_check_one_threshold(m, name, th, value);
      if (status < 0) {
        ERROR("ut_check_metric_threshold: ut_check_one_threshold failed.");
        STRBUF_DESTROY(buf);
        return -1;
      }

      if (worst_state < status) {
        worst_state = status;
        worst_th = th;
      }
      nmatches++;
    }

    th = th->next;
  } /* while (th) */

  if (nmatches > 0) {
    status = ut_report_state(m, name, worst_th, value, worst_state);
    if (status != 0) {
      ERROR("ut_check_metric_threshold: ut_report_state failed.");
      STRBUF_DESTROY(buf);
      return -1;
    }
  }

  STRBUF_DESTROY(buf);
  return 0;
} /* }}} ut_check_metric_threshold */

/*
 * int ut_check_threshold
 *
 * Gets a list of matching thresholds and searches for the worst status by one
 * of the thresholds. Then reports that status using the ut_report_state
 * function above.
 * Returns zero on success and if no threshold has been configured. Returns
 * less than zero on failure.
 */
static int ut_check_threshold(metric_family_t const *fam,
                              __attribute__((unused))
                              user_data_t *ud) { /* {{{ */
  if (threshold_tree == NULL)
    return 0;

  threshold_t *th = threshold_get(fam->name);
  if (th == NULL)
    return 0;

  DEBUG("ut_check_threshold: Found matching threshold(s)");

  for (size_t i = 0; i < fam->metric.num; i++) {
    metric_t const *m = fam->metric.ptr + i;
    ut_check_metric_threshold(m, th);
  }

  return 0;
} /* }}} int ut_check_threshold */


static void ut_metric_missing(metric_t const *m, threshold_t *th) {  /* }}} */
  /* dispatch notifications for "interesting" values only */
  bool found_interesting = false;
  while (th != NULL) {
    bool match = true;

    for (size_t i = 0; i < th->labels.num; i++) {
      char const *value = metric_label_get(m, th->labels.ptr[i].name);
      if (value == NULL) {
        match = false;
        break;
      }
      if (strcmp(th->labels.ptr[i].value, value) !=0) {
        match = false;
        break;
      }
    }

    if (match && (th->flags & UT_FLAG_INTERESTING)) {
      found_interesting = true;
      break;
    }
    th = th->next;
  } /* while (th) */

  if (!found_interesting)
    return;

  cdtime_t now = cdtime();
  cdtime_t missing_time = now - m->time;

  strbuf_t buf = STRBUF_CREATE;
  int status = metric_identity(&buf, m);
  if (status != 0) {
    STRBUF_DESTROY(buf);
    return;
  }
  const char *name = buf.ptr;

  notification_t n = {0};
  notification_init_metric(&n, NOTIF_FAILURE, m);

  strbuf_t buf_summary = STRBUF_CREATE;
  strbuf_printf(&buf_summary, "%s has not been updated for %.3f seconds.", name, CDTIME_T_TO_DOUBLE(missing_time));
  notification_annotation_set(&n, "summary", buf_summary.ptr);

  n.time = now;

  plugin_dispatch_notification(&n);
  notification_reset(&n);

  STRBUF_DESTROY(buf);
  STRBUF_DESTROY(buf_summary);
} /* }}} void ut_metric_missing */

/*
 * int ut_missing
 *
 * This function is called whenever a value goes "missing".
 */
static int ut_missing(metric_family_t const *fam,
                      __attribute__((unused)) user_data_t *ud) { /* {{{ */
  if (threshold_tree == NULL)
    return 0;

  threshold_t *th = threshold_get(fam->name);

  for (size_t i = 0; i < fam->metric.num; i++) {
    metric_t const *m = fam->metric.ptr + i;
    ut_metric_missing(m, th);
  }

  return 0;
} /* }}} int ut_missing */

static int ut_config(oconfig_item_t *ci) { /* {{{ */
  int status = 0;
  int old_size = c_avl_size(threshold_tree);

  if (threshold_tree == NULL) {
    threshold_tree = c_avl_create((int (*)(const void *, const void *))strcmp);
    if (threshold_tree == NULL) {
      ERROR("ut_config: c_avl_create failed.");
      return -1;
    }
  }

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *option = ci->children + i;

    if (strcasecmp("Metric", option->key) == 0)
      status = ut_config_metric(option);
    else {
      WARNING("threshold values: Option `%s' not allowed here.", option->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  /* register callbacks if this is the first time we see a valid config */
  if ((old_size == 0) && (c_avl_size(threshold_tree) > 0)) {
    plugin_register_missing("threshold", ut_missing,
                            /* user data = */ NULL);
    plugin_register_write("threshold", ut_check_threshold,
                          /* user data = */ NULL);
  }

  return status;
} /* }}} int um_config */

void module_register(void) {
  plugin_register_complex_config("threshold", ut_config);
}
