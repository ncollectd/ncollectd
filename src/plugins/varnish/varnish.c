// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2010      Jérôme Renard
// Copyright (C) 2010      Marc Fournier
// Copyright (C) 2010-2012 Florian Forster
// Authors:
//   Jérôme Renard <jerome.renard at gmail.com>
//   Marc Fournier <marc.fournier at camptocamp.com>
//   Florian octo Forster <octo at collectd.org>
//   Denes Matetelki <dmatetelki at varnish-software.com>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#if HAVE_VARNISH_V4 || HAVE_VARNISH_V5 || HAVE_VARNISH_V6
#include <vapi/vsc.h>
#include <vapi/vsm.h>
typedef struct VSC_C_main c_varnish_stats_t;
#endif

#if HAVE_VARNISH_V3
#include <varnishapi.h>
#include <vsc.h>
typedef struct VSC_C_main c_varnish_stats_t;
#endif

#include "varnish_fam.h"
#include "varnish_stats.h"

typedef struct {
  char *instance;
  char *vsh_instance;
  label_set_t labels;
  metric_family_t fams[FAM_VARNISH_MAX];
} varnish_instance_t;

static int varnish_monitor(void *priv, const struct VSC_point *const pt)
{
  char *tokens[12] = {NULL};
  size_t tokens_num = 0;

  if (pt == NULL)
    return 0;

#if HAVE_VARNISH_V6 | HAVE_VARNISH_V5
  char buffer[1024];
  sstrncpy(buffer, pt->name, sizeof(buffer));

  char *ptr = buffer;

  tokens[tokens_num] = ptr;
  tokens_num++;

  char *end = NULL;
  int sep = '.';
  while ((end = strchr(ptr, sep)) != NULL) {
    *(end++) = '\0';
    if ((sep == ')') && (*end == '.')) end++;
    ptr = end;
    if (*ptr == '(') {
      sep = ')';
      if (tokens_num < STATIC_ARRAY_SIZE(tokens)) {
        tokens[tokens_num] = ++ptr;
        tokens_num++;
      }
    } else {
      sep = '.';
      if (tokens_num < STATIC_ARRAY_SIZE(tokens)) {
         tokens[tokens_num] = ptr;
          tokens_num++;
      }
    }
  }

  if ((tokens_num < 2) || (tokens_num > STATIC_ARRAY_SIZE(tokens))) {
    return 0;
  }

#elif HAVE_VARNISH_V4
  if (pt->section->fantom->type[0] == 0)
    return 0;

  tokens[0] = pt->section->fantom->type;
  tokens_num++;
 
  if (pt->section->fantom->ident[0] != 0) {
    tokens[tokens_num] = pt->section->fantom->ident;
    tokens_num++;
  }

  tokens[tokens_num] = pt->desc->name;
  tokens_num++;
#elif HAVE_VARNISH_V3
  if (pt->class[0] == 0)
    return 0;

  tokens[0] = pt->class;
  tokens_num++;
 
  if (pt->ident[0] != 0) {
    tokens[tokens_num] = pt->ident;
    tokens_num++;
  }

  tokens[tokens_num] = pt->name;
  tokens_num++;
#endif

  varnish_instance_t *conf = priv;
  uint64_t val = *(const volatile uint64_t *)pt->ptr;

  char mname[256];
  ssnprintf(mname, sizeof(mname), "%s.%s", tokens[0], tokens[tokens_num-1]);
  // FIXME check overflow

  const struct varnish_stats_metric *vsh_metric = varnish_stats_get_key (mname, strlen(mname));
  if (vsh_metric != NULL) {
    metric_family_t *fam = &(conf->fams[vsh_metric->fam]);

    /* special cases */
    if ((strcmp(tokens[0], "VBE") == 0) &&
        (strcmp(tokens[2], "goto") == 0) && (tokens_num >= 7)) {
      tokens[2] = tokens[4];
      tokens[3] = tokens[5];
    } else if ((strcmp(tokens[0], "LCK") == 0) && (tokens_num >= 4)) {
      char *swap = tokens[1];
      tokens[1] = tokens[2];
      tokens[2] = swap;
    }

    metric_t m = {0};
    if (vsh_metric->fam == FAM_VARNISH_BACKEND_UP) {
      m.value.gauge.float64 = val & 1;
    } else {
      if (fam->type == METRIC_TYPE_GAUGE)
        m.value.gauge.float64 = (double)val;
      else
        m.value.counter.uint64 = val;
    }
  
    metric_label_set(&m, "instance", conf->instance);

    for (size_t i = 0; i < conf->labels.num; i++) {
      metric_label_set(&m, conf->labels.ptr[i].name, conf->labels.ptr[i].value);
    }

    if (vsh_metric->lkey != NULL)
      metric_label_set(&m, vsh_metric->lkey, vsh_metric->lvalue);
    if ((vsh_metric->tag1 != NULL) && (tokens_num > 2))
      metric_label_set(&m, vsh_metric->tag1, tokens[1]);
    if ((vsh_metric->tag2 != NULL) && (tokens_num > 3))
      metric_label_set(&m, vsh_metric->tag2, tokens[2]);
    if ((vsh_metric->tag3 != NULL) && (tokens_num > 4))
      metric_label_set(&m, vsh_metric->tag3, tokens[3]);

    metric_family_metric_append(fam, m);

    metric_reset(&m);
  }

  return 0;
}

static int varnish_read_instance(varnish_instance_t *conf)
{
#if HAVE_VARNISH_V3 || HAVE_VARNISH_V4
  struct VSM_data *vd;
  bool ok;
  const c_varnish_stats_t *stats;
#elif HAVE_VARNISH_V5 || HAVE_VARNISH_V6
  struct vsm *vd;
  struct vsc *vsc;
  int vsm_status;
#endif

  vd = VSM_New();
#if HAVE_VARNISH_V5 || HAVE_VARNISH_V6
  vsc = VSC_New();
#endif
#if HAVE_VARNISH_V3
  VSC_Setup(vd);
#endif

  if (conf->vsh_instance != NULL) {
    int status;

#if HAVE_VARNISH_V3 || HAVE_VARNISH_V4
    status = VSM_n_Arg(vd, conf->vsh_instance);
#elif HAVE_VARNISH_V5 || HAVE_VARNISH_V6
    status = VSM_Arg(vd, 'n', conf->vsh_instance);
#endif

    if (status < 0) {
#if HAVE_VARNISH_V3 || HAVE_VARNISH_V4
      VSM_Delete(vd);
#elif HAVE_VARNISH_V5 || HAVE_VARNISH_V6
      VSC_Destroy(&vsc, vd);
      VSM_Destroy(&vd);
#endif
      ERROR("varnish plugin: VSM_Arg (\"%s\") failed "
            "with status %i.",
            conf->vsh_instance, status);
      return -1;
    }
  }

#if HAVE_VARNISH_V3
  ok = (VSC_Open(vd, /* diag = */ 1) == 0);
#elif HAVE_VARNISH_V4
  ok = (VSM_Open(vd) == 0);
#endif
#if HAVE_VARNISH_V3 || HAVE_VARNISH_V4
  if (!ok) {
    VSM_Delete(vd);
    ERROR("varnish plugin: Unable to open connection.");
    return -1;
  }
#endif

#if HAVE_VARNISH_V3
  stats = VSC_Main(vd);
#elif HAVE_VARNISH_V4
  stats = VSC_Main(vd, NULL);
#endif
#if HAVE_VARNISH_V3 || HAVE_VARNISH_V4
  if (!stats) {
    VSM_Delete(vd);
    ERROR("varnish plugin: Unable to get statistics.");
    return -1;
  }
#endif

#if HAVE_VARNISH_V5 || HAVE_VARNISH_V6
  if (VSM_Attach(vd, STDERR_FILENO)) {
    ERROR("varnish plugin: Cannot attach to varnish. %s", VSM_Error(vd));
    VSC_Destroy(&vsc, vd);
    VSM_Destroy(&vd);
    return -1;
  }

  vsm_status = VSM_Status(vd);
  if (vsm_status & ~(VSM_MGT_RUNNING | VSM_WRK_RUNNING)) {
    ERROR("varnish plugin: Unable to get statistics.");
    VSC_Destroy(&vsc, vd);
    VSM_Destroy(&vd);
    return -1;
  }
#endif

#if HAVE_VARNISH_V3
  VSC_Iter(vd, varnish_monitor, conf);
#elif HAVE_VARNISH_V4
  VSC_Iter(vd, NULL, varnish_monitor, conf);
#elif HAVE_VARNISH_V5 || HAVE_VARNISH_V6
  VSC_Iter(vsc, vd, varnish_monitor, conf);
#endif

#if HAVE_VARNISH_V3 || HAVE_VARNISH_V4
  VSM_Delete(vd);
#elif HAVE_VARNISH_V5 || HAVE_VARNISH_V6
  VSC_Destroy(&vsc, vd);
  VSM_Destroy(&vd);
#endif

  return 0;
}

static int varnish_read(user_data_t *ud)
{
  if ((ud == NULL) || (ud->data == NULL))
    return EINVAL;

  varnish_instance_t *conf = ud->data;
  
  int status = varnish_read_instance(conf);

  metric_t m = {0};
  metric_label_set(&m, "instance", conf->instance);
  for (size_t i = 0; i < conf->labels.num; i++) {
    metric_label_set(&m, conf->labels.ptr[i].name, conf->labels.ptr[i].value);
  }
  if (status < 0) {
    m.value.gauge.float64 = 0;
  } else {
    m.value.gauge.float64 = 1;
  }
  metric_family_metric_append(&conf->fams[FAM_VARNISH_UP], m);
  metric_reset(&m);

  plugin_dispatch_metric_family_array(conf->fams, FAM_VARNISH_MAX);
  return 0;
}

static void varnish_config_free(void *ptr) 
{
  varnish_instance_t *conf = ptr;

  if (conf == NULL)
    return;

  if (conf->instance != NULL)
    free(conf->instance);
  if (conf->vsh_instance != NULL)
    free(conf->vsh_instance);
  label_set_reset(&conf->labels);

  free(conf);
} 

static int varnish_config_instance(const oconfig_item_t *ci)
{
  varnish_instance_t *conf = calloc(1, sizeof(*conf));
  if (conf == NULL)
    return ENOMEM;
  conf->instance = NULL;

  memcpy(conf->fams, fams, FAM_VARNISH_MAX * sizeof(conf->fams[0]));

  if (ci->values_num == 1) {
    int status;

    status = cf_util_get_string(ci, &conf->instance);
    if (status != 0) {
      sfree(conf);
      return status;
    }
    assert(conf->instance != NULL);

  } else if (ci->values_num > 1) {
    WARNING("Varnish plugin: \"Instance\" blocks accept only "
            "one argument.");
    sfree(conf);
    return EINVAL;
  }

  cdtime_t interval = 0;
  int status = 0;
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    if (strcasecmp("Instance", child->key) == 0)
      status = cf_util_get_string(child, &conf->vsh_instance);
    else if (strcasecmp("Interval", child->key) == 0)
      status = cf_util_get_cdtime(child, &interval);
    else if (strcasecmp("Label", child->key) == 0)
      status = cf_util_get_label(child, &conf->labels);
    else {
      WARNING("haproxy plugin: Option `%s' not allowed here.", child->key);
      status = -1;
    }

    if (status != 0)
      break;
  }

  if (status != 0) {
    varnish_config_free(conf);
    return status;
  }

  if (conf->vsh_instance != NULL) {
    if (strcmp("localhost", conf->vsh_instance) == 0) {
      sfree(conf->vsh_instance);
      conf->vsh_instance = NULL;
    }
  }

  char callback_name[DATA_MAX_NAME_LEN];
  ssnprintf(callback_name, sizeof(callback_name), "varnish/%s",
            (conf->vsh_instance == NULL) ? "localhost" : conf->vsh_instance);

  plugin_register_complex_read(
      /* group = */ "varnish",
      /* name      = */ callback_name,
      /* callback  = */ varnish_read,
      /* interval  = */ interval,
      &(user_data_t){
          .data = conf,
          .free_func = varnish_config_free,
      });

  return 0;
}

static int varnish_config(oconfig_item_t *ci)
{
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Instance", child->key) == 0)
      varnish_config_instance(child);
    else {
      WARNING("Varnish plugin: Ignoring unknown configuration option: \"%s\"",
              child->key);
    }
  }

  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("varnish", varnish_config);
}
