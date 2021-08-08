#include "metric.h"
#include "metric_chars.h"
#include "metric_match.h"

#include <regex.h>

static int metric_match_value_alloc(metric_match_op_t op, metric_match_value_t *value, char const *str)
{
  if ((op == METRIC_MATCH_OP_EQL_REGEX) ||
      (op == METRIC_MATCH_OP_NEQ_REGEX)) {
    int status = regcomp(value->regex, str, REG_EXTENDED|REG_NOSUB);
    if (status != 0)
      return errno;
  } else if ((op == METRIC_MATCH_OP_EQL) ||
             (op == METRIC_MATCH_OP_NEQ)) {
    value->string = strdup(str);
    if (value->string == NULL)
      return ENOMEM;
  }

  return 0;
}

int metric_match_set_add(metric_match_set_t *match, char const *name, metric_match_op_t op, char *value)
{
  if ((match == NULL) || (name == NULL))
    return EINVAL;

  metric_match_pair_t *tmp = realloc(match->ptr, sizeof(*match->ptr) * (match->num + 1));
  if (tmp == NULL)
    return errno;
  match->ptr = tmp;

  metric_match_pair_t pair = {.name = strdup(name), .op = op};
  if (pair.name == NULL)
    return ENOMEM;
  
  int status = metric_match_value_alloc(op, &pair.value, value);
  if (status != 0) {
    free(pair.name);
    return status;
  }

  match->ptr[match->num] = pair;
  match->num++;

  return 0;
}

int metric_match_add(metric_match_t *match, char const *name, metric_match_op_t op, char *value)
{
  if ((match == NULL) || (name == NULL))
    return EINVAL;

  if (strcmp("__name__", name) == 0)
    return metric_match_set_add(&match->family, name, op, value);

  return metric_match_set_add(&match->labels, name, op, value);
}

void metric_match_set_reset(metric_match_set_t *match_set)
{
  if (match_set == NULL)
    return;

  if (match_set->num > 0) {
    for (size_t i = 0; i < match_set->num; i++) {
      free(match_set->ptr[i].name);
      switch (match_set->ptr[i].op) {
        case METRIC_MATCH_OP_NONE:
          break;
        case METRIC_MATCH_OP_EQL:
        case METRIC_MATCH_OP_NEQ:
          free(match_set->ptr[i].value.string);
          break;
        case METRIC_MATCH_OP_EQL_REGEX:
        case METRIC_MATCH_OP_NEQ_REGEX:
          regfree(match_set->ptr[i].value.regex);
          break;
        case METRIC_MATCH_OP_EXISTS:
          break;
        case METRIC_MATCH_OP_NEXISTS:
          break;
      }
    }
    free(match_set->ptr);
    match_set->ptr = NULL;
    match_set->num = 0;
  }
}

void metric_match_reset(metric_match_t *match)
{
  if (match == NULL)
    return;

  metric_match_set_reset(&match->family);
  metric_match_set_reset(&match->labels);
}

static int parse_label_value(strbuf_t *buf, char const **inout)
{
  char const *ptr = *inout;

  if (ptr[0] != '"')
    return EINVAL;
  ptr++;

  while (ptr[0] != '"') {
    size_t valid_len = strcspn(ptr, "\\\"\n");
    if (valid_len != 0) {
      strbuf_printn(buf, ptr, valid_len);
      ptr += valid_len;
      continue;
    }

    if ((ptr[0] == 0) || (ptr[0] == '\n')) {
      return EINVAL;
    }

    assert(ptr[0] == '\\');
    if (ptr[1] == 0)
      return EINVAL;

    char tmp[2] = {ptr[1], 0};
    if (tmp[0] == 'n') {
      tmp[0] = '\n';
    } else if (tmp[0] == 'r') {
      tmp[0] = '\r';
    } else if (tmp[0] == 't') {
      tmp[0] = '\t';
    }

    strbuf_print(buf, tmp);

    ptr += 2;
  }

  assert(ptr[0] == '"');
  ptr++;
  *inout = ptr;
  return 0;
}


int metric_match_unmarshal(metric_match_t *match, char const *str)
{
  int ret = 0;
  char const *ptr = str;

  size_t name_len = metric_valid_len(ptr);
  if (name_len != 0) {
    char name[name_len + 1];
    strncpy(name, ptr, name_len);
    name[name_len] = 0;
    ptr += name_len;

    int status = metric_match_set_add(&match->family, "__name__", METRIC_MATCH_OP_EQL, name);
    if (status != 0)
      return status;

    /* metric name without labels */
    if ((ptr[0] == 0) || (ptr[0] == ' '))
      return 0;
  }

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

    metric_match_op_t op;
    if (ptr[0] == '=') {
      op = METRIC_MATCH_OP_EQL;
      ptr++;
      if (ptr[0] == '~') {
        op = METRIC_MATCH_OP_EQL_REGEX;
        ptr++;
      }
    } else if (ptr[0] == '!') {
      op = METRIC_MATCH_OP_NEQ;
      if (ptr[0] == '~') {
        op = METRIC_MATCH_OP_NEQ_REGEX;
        ptr++;
      }
    } else {
      ret = EINVAL;
      break;
    }
    
    strbuf_reset(&value);   
    int status = parse_label_value(&value, &ptr);
    if (status != 0) {
      ret = status;
      break;
    }

    if (value.ptr[0] == '\0') {
      if (op == METRIC_MATCH_OP_EQL) {
        op = METRIC_MATCH_OP_NEXISTS;
      } else if (op == METRIC_MATCH_OP_NEQ) {
        op = METRIC_MATCH_OP_EXISTS;
      }
    } 
  
    if ((key_len == strlen("__name__")) && !strcmp("__name__", key)) {
      status = metric_match_set_add(&match->family, key, op, value.ptr);
      if (status != 0) {
        ret = status;
        break;
      }
    } else {
      status = metric_match_set_add(&match->labels, key, op, value.ptr);
      if (status != 0) {
        ret = status;
        break;
      }
    }
  }
  strbuf_destroy(&value);

  if (ret != 0)
    return ret;

  if (ptr[0] != '}')
    return EINVAL;

  return 0;
}

static inline int metric_match_value_cmp(metric_match_value_t value, metric_match_op_t op, char const *name)
{
  switch (op) {
    case METRIC_MATCH_OP_NONE:
      break;
    case METRIC_MATCH_OP_EQL:
      return strcmp(name, value.string) == 0;
      break;
    case METRIC_MATCH_OP_NEQ:
      return strcmp(name, value.string) != 0;
      break;
    case METRIC_MATCH_OP_EQL_REGEX:
      return regexec(value.regex, name, 0, NULL, 0) == 0;
      break;
    case METRIC_MATCH_OP_NEQ_REGEX:
      return regexec(value.regex, name, 0, NULL, 0) != 0;
      break;
    case METRIC_MATCH_OP_EXISTS:
      break;
    case METRIC_MATCH_OP_NEXISTS:
      break;
  }
  return -1;
}

static inline int metric_match_labels_cmp(metric_match_set_t *match_set, metric_t *m)
{
  for (size_t i = 0; i < match_set->num; i++) {
    label_pair_t *pair = label_set_read(m->label, match_set->ptr[i].name);

    if (match_set->ptr[i].op == METRIC_MATCH_OP_EXISTS) {
      if (pair == NULL)
        return -1;
    } else if (match_set->ptr[i].op == METRIC_MATCH_OP_NEXISTS) {
      if (pair != NULL)
        return -1;
    } else {
      if (pair == NULL)
        return -1;
      if (metric_match_value_cmp(match_set->ptr[i].value, match_set->ptr[i].op, pair->value) != 0)
        return -1;
    }
  }

  return 0;
}

static inline int metric_match_family_cmp(metric_match_set_t *match_set, metric_family_t *fam)
{
  for (size_t i = 0; i < match_set->num; i++) {
    if (match_set->ptr[i].op == METRIC_MATCH_OP_EXISTS) {
      /* always have family */
    } else if (match_set->ptr[i].op == METRIC_MATCH_OP_NEXISTS) {
      return -1;
    } else {
      if (metric_match_value_cmp(match_set->ptr[i].value, match_set->ptr[i].op, fam->name) != 0)
        return -1;
    }
  }

  return 0;
}

int metric_match_exec(metric_match_t *match, metric_family_t *fam, metric_family_t **fam_ret)
{
  *fam_ret = NULL;

  if ((match == NULL) || (fam == NULL))
    return EINVAL;
  
  int status = metric_match_family_cmp(&match->family, fam);
  if (status != 0)
    return status;
  
  *fam_ret = calloc(1, sizeof(**fam_ret));
  if (fam_ret == NULL)
    return ENOMEM;
 
  (*fam_ret)->name = strdup(fam->name);
  if (fam->help != NULL)
    (*fam_ret)->help = strdup(fam->help);
  if (fam->unit != NULL)
    (*fam_ret)->unit = strdup(fam->unit);
  (*fam_ret)->type = fam->type;
 
  metric_list_t *metrics = &fam->metric;
  for (size_t i = 0; i < metrics->num; i++) {
    if (metric_match_labels_cmp(&match->labels, &metrics->ptr[i]) == 0) {
       metric_family_metric_append(*fam_ret,  metrics->ptr[i]);
    }
  }

  return 0;
}
