// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009  Sven Trenkel
// SPDX-FileContributor: Sven Trenkel <collectd at semidefinite.de>

#include <Python.h>
#include <structmember.h>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/itoa.h"
#include "libutils/dtoa.h"

#include "cpython.h"

#pragma GCC diagnostic ignored "-Wcast-function-type"

static char fam_name_doc[] = "The name of the metric family.";

static char fam_help_doc[] = "Brief description of the metric family.";

static char fam_unit_doc[] = "Specifies the metric family units.";

static char fam_metrics_doc[] = "List of metrics.";

static char MetricFamily_doc[] =
    "The MetricFamily class is a wrapper around the ncollectd metric_family_t.\n"
    "It can be used to submit metrics.\n"
    "Metric families can be dispatched at any time and can be received with "
    "register_read.";

static char fam_dispatch_doc[] =
    "dispatch([metrics]) -> None.  Dispatch metrics.\n"
    "\n"
    "Dispatch this metrics to the ncollectd process.\n"
    "\n"
    "If you do not submit the metrics the value saved in its member will be submitted.\n"
    "If you do provide the metrics it will be used instead, "
    "without altering the metrics in the metrics family.";

static char fam_append_doc[] =
    "append([metrics]) -> None.  Append metrics.\n"
    "\n"
    "Append this metrics to this metrics family.\n";

static int MetricFamily_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricFamily *self = (MetricFamily *)s;
    int type = METRIC_TYPE_UNKNOWN;
    PyObject *name = NULL;
    PyObject *help = NULL;
    PyObject *unit = NULL;
    PyObject *metrics= NULL;

    static char *kwlist[] = {"type", "name", "help", "unit", "metrics", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "iO|OOO", kwlist,
                                             &type, &name, &help, &unit, &metrics);
    if (status == 0)
        return -1;

    if (!IS_BYTES_OR_UNICODE(name)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be a str");
        Py_XDECREF(name);
        Py_XDECREF(help);
        Py_XDECREF(unit);
        Py_XDECREF(metrics);
        return -1;
    }

    if ((help != NULL) && !IS_BYTES_OR_UNICODE(name)) {
        PyErr_SetString(PyExc_TypeError, "help must be a str");
        Py_XDECREF(name);
        Py_XDECREF(help);
        Py_XDECREF(unit);
        Py_XDECREF(metrics);
        return -1;
    }

    if ((unit != NULL) && !IS_BYTES_OR_UNICODE(unit)) {
        PyErr_SetString(PyExc_TypeError, "unit must be a str");
        Py_XDECREF(name);
        Py_XDECREF(help);
        Py_XDECREF(unit);
        Py_XDECREF(metrics);
        return -1;
    }

    if ((metrics != NULL) && (!PyTuple_Check(metrics) && !PyList_Check(metrics))) {
        PyErr_SetString(PyExc_TypeError, "metrics must be a list or a tuple");
        Py_XDECREF(name);
        Py_XDECREF(help);
        Py_XDECREF(unit);
        Py_XDECREF(metrics);
        return -1;
    }

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
    case METRIC_TYPE_GAUGE:
    case METRIC_TYPE_COUNTER:
    case METRIC_TYPE_STATE_SET:
    case METRIC_TYPE_INFO:
    case METRIC_TYPE_SUMMARY:
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        break;
    default:
        PyErr_Format(PyExc_TypeError, "invalid metric type: %d", type);
        Py_XDECREF(name);
        Py_XDECREF(help);
        Py_XDECREF(unit);
        Py_XDECREF(metrics);
        return -1;
        break;
    }
    self->type = type;


    PyObject *old_name = self->name;
    Py_INCREF(name);
    self->name = name;
    Py_XDECREF(old_name);

    if (help != NULL) {
        PyObject *old_help = self->help;
        Py_INCREF(help);
        self->help = help;
        Py_XDECREF(old_help);
    }

    if (unit != NULL) {
        PyObject *old_unit = self->unit;
        Py_INCREF(unit);
        self->unit = unit;
        Py_XDECREF(old_unit);
    }

    if (metrics == NULL) {
        metrics = PyList_New(0); /* New reference. */
        PyErr_Clear();
        PyObject *old_metrics = self->metrics;
        self->metrics = metrics;
        Py_XDECREF(old_metrics);
    } else {
        PyObject *old_metrics = self->metrics;
        Py_INCREF(metrics);
        self->metrics = metrics;
        Py_XDECREF(old_metrics);
    }

    return 0;
}

static int build_metric(metric_t *m, int metric_type, PyObject *item)
{
    PyTypeObject *type = Py_TYPE(item);
    Metric *cpy_metric = NULL;

    switch (metric_type) {
    case METRIC_TYPE_UNKNOWN:
        if (strcmp(type->tp_name, "ncollectd.MetricUnknownDouble") == 0) {
            MetricUnknownDouble *unknown = (MetricUnknownDouble *) item;
            m->value = VALUE_UNKNOWN_FLOAT64(unknown->value);
            cpy_metric = &unknown->metric;
        } else if (strcmp(type->tp_name, "ncollectd.MetricUnknownLong") == 0) {
            MetricUnknownLong *unknown = (MetricUnknownLong*) item;
            m->value = VALUE_UNKNOWN_INT64(unknown->value);
            cpy_metric = &unknown->metric;
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (strcmp(type->tp_name, "ncollectd.MetricGaugeDouble") == 0) {
            MetricGaugeDouble *gauge = (MetricGaugeDouble *) item;
            m->value = VALUE_GAUGE_FLOAT64(gauge->value);
            cpy_metric = &gauge->metric;
        } else if (strcmp(type->tp_name, "ncollectd.MetricGaugeLong") == 0) {
            MetricGaugeLong *gauge = (MetricGaugeLong *) item;
            m->value = VALUE_GAUGE_INT64(gauge->value);
            cpy_metric = &gauge->metric;
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (strcmp(type->tp_name, "ncollectd.MetricCounterULong") == 0) {
            MetricCounterULong *counter = (MetricCounterULong *) item;
            m->value = VALUE_COUNTER_UINT64(counter->value);
            cpy_metric = &counter->metric;
        } else if (strcmp(type->tp_name, "ncollectd.MetricCounterDouble") == 0) {
            MetricCounterDouble *counter = (MetricCounterDouble *) item;
            m->value = VALUE_COUNTER_FLOAT64(counter->value);
            cpy_metric = &counter->metric;
        }
        break;
    case METRIC_TYPE_STATE_SET:
        if (strcmp(type->tp_name, "ncollectd.MetricStateSet") == 0) {
            MetricStateSet *set = (MetricStateSet *) item;
            cpy_metric = &set->metric;
            cpy_build_state_set(set->set, &m->value.state_set);
        }
        break;
    case METRIC_TYPE_INFO:
        if (strcmp(type->tp_name, "ncollectd.MetricInfo") == 0) {
            MetricInfo *info = (MetricInfo *) item;
            cpy_metric = &info->metric;
            cpy_build_labels(info->info, &m->value.info);
        }
        break;
    case METRIC_TYPE_SUMMARY:
        if (strcmp(type->tp_name, "ncollectd.MetricSummary") == 0) {
            MetricSummary *summary = (MetricSummary *) item;
            cpy_metric = &summary->metric;
            m->value.summary = cpy_build_summary (summary->quantiles);
            if (m->value.summary == NULL) {
                cpy_metric = NULL;
            } else {
                m->value.summary->sum = summary->sum;
                m->value.summary->count = summary->count;
            }
        }
        break;
    case METRIC_TYPE_HISTOGRAM:
        if (strcmp(type->tp_name, "ncollectd.MetricHistogram") == 0) {
            MetricHistogram *histogram = (MetricHistogram *) item;
            cpy_metric = &histogram->metric;
            m->value.histogram = cpy_build_histogram(histogram->buckets);
            if (m->value.histogram == NULL) {
                cpy_metric = NULL;
            } else {
                m->value.histogram->sum = histogram->sum;
            }
        }
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        if (strcmp(type->tp_name, "ncollectd.MetricGaugeHistogram") == 0) {
            MetricGaugeHistogram *gauge_histogram = (MetricGaugeHistogram *) item;
            cpy_metric = &gauge_histogram->metric;
            m->value.histogram = cpy_build_histogram(gauge_histogram->buckets);
            if (m->value.histogram == NULL) {
                cpy_metric = NULL;
            } else {
                m->value.histogram->sum = gauge_histogram->sum;
            }
        }
        break;
    }

    if (cpy_metric == NULL)
        return -1;

    m->time = DOUBLE_TO_CDTIME_T(cpy_metric->time);
    m->interval = DOUBLE_TO_CDTIME_T(cpy_metric->interval);

    cpy_build_labels(cpy_metric->labels, &m->label);

    return 0;
}

static PyObject *MetricFamily_dispatch(PyObject *s, PyObject *args, PyObject *kwds)
{
    int ret;
    MetricFamily *self = (MetricFamily *) s;
    PyObject *metrics = NULL;
    double time = 0;
    static char *kwlist[] = {"metrics", "time" , NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "|Od", kwlist, &metrics, &time);
    if (status == 0)
        return NULL;

    if (self->name == NULL) {
        Py_XDECREF(metrics);
        PyErr_SetString(PyExc_TypeError, "missing name");
        return NULL;
    }

    if (!IS_BYTES_OR_UNICODE(self->name)) {
        Py_XDECREF(metrics);
        PyErr_SetString(PyExc_TypeError, "name must be str");
        return NULL;
    }

    if ((self->help != NULL) && !IS_BYTES_OR_UNICODE(self->help)) {
        Py_XDECREF(metrics);
        PyErr_SetString(PyExc_TypeError, "help must be str");
        return NULL;
    }

    if ((self->unit != NULL) && !IS_BYTES_OR_UNICODE(self->unit)) {
        Py_XDECREF(metrics);
        PyErr_SetString(PyExc_TypeError, "unit must be str");
        return NULL;
    }

    switch (self->type) {
    case METRIC_TYPE_UNKNOWN:
    case METRIC_TYPE_GAUGE:
    case METRIC_TYPE_COUNTER:
    case METRIC_TYPE_STATE_SET:
    case METRIC_TYPE_INFO:
    case METRIC_TYPE_SUMMARY:
    case METRIC_TYPE_HISTOGRAM:
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        break;
    default:
        Py_XDECREF(metrics);
        PyErr_SetString(PyExc_TypeError, "invalid metric type value");
        return NULL;
        break;
    }

    metric_family_t fam = {0};

    fam.type = self->type;

    if (metrics == NULL) {
        if (self->metrics == NULL)
            Py_RETURN_NONE;

        if ((!PyTuple_Check(self->metrics) && !PyList_Check(self->metrics))) {
            PyErr_Format(PyExc_TypeError, "metrics must be a list");
            return NULL;
        }
    } else if ((metrics != NULL) && (!PyTuple_Check(metrics) && !PyList_Check(metrics))) {
        Py_XDECREF(metrics);
        PyErr_Format(PyExc_TypeError, "metrics must be a list");
        return NULL;
    }

    PyObject *str_unit = NULL;
    if (self->unit != NULL) {
        str_unit = PyObject_Str(self->unit);
        const char *string = cpy_unicode_or_bytes_to_string(&str_unit);
        if (string == NULL) {
            Py_XDECREF(str_unit);
            str_unit = NULL;
        } else {
            fam.unit = discard_const(string);
        }
    }

    PyObject *str_help = NULL;
    if (self->help != NULL) {
        str_help = PyObject_Str(self->help);
        const char *string = cpy_unicode_or_bytes_to_string(&str_help);
        if (string == NULL) {
            Py_XDECREF(str_help);
            str_help = NULL;
        } else {
            fam.help = discard_const(string);
        }
    }

    PyObject *str_name = PyObject_Str(self->name);
    const char *string = cpy_unicode_or_bytes_to_string(&str_name);
    if (string == NULL) {
        Py_XDECREF(str_name);
        str_name = NULL;
    } else {
        fam.name = discard_const(string);
    }

    PyObject *metrics_list = metrics == NULL ? self->metrics : metrics;

    size_t size = (size_t)PySequence_Length(metrics_list);
    fam.metric.ptr = calloc(size, sizeof(*fam.metric.ptr));
    if (fam.metric.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "failed to alloc metrics");
        Py_XDECREF(metrics);
        Py_XDECREF(str_name);
        Py_XDECREF(str_help);
        Py_XDECREF(str_unit);
        return NULL;
    }

    fam.metric.num = size;

    for (size_t i = 0; i < size; ++i) {
        PyObject *item = PySequence_Fast_GET_ITEM(metrics_list, (int)i); /* Borrowed reference. */
        if (item == NULL) {
            Py_XDECREF(metrics);
            Py_XDECREF(str_name);
            Py_XDECREF(str_help);
            Py_XDECREF(str_unit);
            metric_list_reset(&fam.metric, fam.type);
            return NULL;
        }

        metric_t *m = fam.metric.ptr + i;

        status = build_metric(m, self->type, item);
        if (status != 0) {
            Py_XDECREF(metrics);
            Py_XDECREF(str_name);
            Py_XDECREF(str_help);
            Py_XDECREF(str_unit);
            metric_list_reset(&fam.metric, fam.type);
            return NULL;
        }

        if (PyErr_Occurred() != NULL) {
            Py_XDECREF(metrics);
            Py_XDECREF(str_name);
            Py_XDECREF(str_help);
            Py_XDECREF(str_unit);
            metric_list_reset(&fam.metric, fam.type);
            return NULL;
        }
    }

    Py_BEGIN_ALLOW_THREADS;
    ret = plugin_dispatch_metric_family(&fam, CDTIME_T_TO_DOUBLE(time));
    Py_END_ALLOW_THREADS;

    if (metrics == NULL) {
        Py_XDECREF(self->metrics);
        self->metrics = PyList_New(0); /* New reference. */
    } else {
        Py_XDECREF(metrics);
    }

    Py_XDECREF(str_name);
    Py_XDECREF(str_help);
    Py_XDECREF(str_unit);

    if (ret != 0) {
        PyErr_SetString(PyExc_RuntimeError, "error dispatching metric family, read the logs");
        return NULL;
    }

    Py_RETURN_NONE;
}

static bool check_metric_type(metric_type_t metric_type, PyObject *item)
{
    PyTypeObject *type = Py_TYPE(item);

    switch (metric_type) {
    case METRIC_TYPE_UNKNOWN:
        if (strcmp(type->tp_name, "ncollectd.MetricUnknownDouble") == 0) {
            return true;
        } else if (strcmp(type->tp_name, "ncollectd.MetricUnknownLong") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (strcmp(type->tp_name, "ncollectd.MetricGaugeDouble") == 0) {
            return true;
        } else if (strcmp(type->tp_name, "ncollectd.MetricGaugeLong") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (strcmp(type->tp_name, "ncollectd.MetricCounterULong") == 0) {
            return true;
        } else if (strcmp(type->tp_name, "ncollectd.MetricCounterDouble") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_STATE_SET:
        if (strcmp(type->tp_name, "ncollectd.MetricStateSet") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_INFO:
        if (strcmp(type->tp_name, "ncollectd.MetricInfo") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_SUMMARY:
        if (strcmp(type->tp_name, "ncollectd.MetricSummary") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_HISTOGRAM:
        if (strcmp(type->tp_name, "ncollectd.MetricHistogram") == 0) {
            return true;
        }
        break;
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        if (strcmp(type->tp_name, "ncollectd.MetricGaugeHistogram") == 0) {
            return true;
        }
        break;
    }
    return false;
}

static PyObject *MetricFamily_append(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricFamily *self = (MetricFamily *) s;
    PyObject *metrics = NULL;
    static char *kwlist[] = {"metrics", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &metrics);
    if (status == 0)
        return NULL;

    if (metrics == NULL)
        Py_RETURN_NONE;

    if (self->metrics == NULL) {
        self->metrics = PyList_New(0); /* New reference. */
        if (self->metrics == NULL)
            return NULL;
    }

    if ((!PyTuple_Check(metrics) && !PyList_Check(metrics))) {
        if (check_metric_type(self->type, metrics) == false) {
            return NULL;
        }

        PyList_Append(self->metrics, metrics);

        Py_RETURN_NONE;
    }

    ssize_t size = PySequence_Length(metrics);

    for (ssize_t i = 0; i < size; ++i) {
        PyObject *item = PySequence_Fast_GET_ITEM(metrics, (int)i); /* Borrowed reference. */
        if (item == NULL) {
            return NULL;
        }

        if (check_metric_type(self->type, item) == false)  {
            return NULL;
        }

        PyList_Append(self->metrics, item);
    }


    Py_RETURN_NONE;
}

static PyObject *MetricFamily_new(PyTypeObject *type, __attribute__((unused)) PyObject *args,
                                  __attribute__((unused)) PyObject *kwds)
{
    MetricFamily *self = (MetricFamily *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->name = NULL;
    self->help = NULL;
    self->unit = NULL;
    self->type = METRIC_TYPE_UNKNOWN;
    self->metrics = PyList_New(0);
    return (PyObject *)self;
}

const char *cpy_metric_type(metric_type_t type)
{
    switch(type) {
    case METRIC_TYPE_UNKNOWN:
        return "METRIC_TYPE_UNKNOWN";
    case METRIC_TYPE_GAUGE:
        return "METRIC_TYPE_GAUGE";
    case METRIC_TYPE_COUNTER:
        return "METRIC_TYPE_COUNTER";
    case METRIC_TYPE_STATE_SET:
        return "METRIC_TYPE_STATE_SET";
    case METRIC_TYPE_INFO:
        return "METRIC_TYPE_INFO";
    case METRIC_TYPE_SUMMARY:
        return "METRIC_TYPE_SUMMARY";
    case METRIC_TYPE_HISTOGRAM:
        return "METRIC_TYPE_HISTOGRAM";
    case METRIC_TYPE_GAUGE_HISTOGRAM:
        return "METRIC_TYPE_GAUGE_HISTOGRAM";
    }
    return NULL;
}

static PyObject *MetricFamily_repr(PyObject *s)
{
    PyObject *tmp;
    static PyObject *l_open, *l_type, *l_name, *l_help, *l_unit, *l_metrics, *l_closing;
    MetricFamily *self = (MetricFamily *)s;

    if (l_open == NULL)
        l_open = cpy_string_to_unicode_or_bytes("(");
    if (l_type == NULL)
        l_type = cpy_string_to_unicode_or_bytes("type=");
    if (l_name == NULL)
        l_name = cpy_string_to_unicode_or_bytes(",name=");
    if (l_help == NULL)
        l_help = cpy_string_to_unicode_or_bytes(",help=");
    if (l_unit== NULL)
        l_unit = cpy_string_to_unicode_or_bytes(",unit=");
    if (l_metrics == NULL)
        l_metrics = cpy_string_to_unicode_or_bytes(",metrics=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_open == NULL) || (l_name == NULL) || (l_help == NULL) || (l_unit == NULL) ||
        (l_metrics == NULL) || (l_closing == NULL))
        return NULL;

    PyObject *ret = cpy_string_to_unicode_or_bytes(s->ob_type->tp_name);

    CPY_STRCAT(&ret, l_open);

    CPY_STRCAT(&ret, l_type);
    const char *metric_type = cpy_metric_type(self->type);
    if (metric_type != NULL) {
        PyObject *type = cpy_string_to_unicode_or_bytes(metric_type);
        if (type != NULL)
            CPY_STRCAT_AND_DEL(&ret, type);
    } else {
        tmp = PyInt_FromLong(self->type);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->name != 0) {
        CPY_STRCAT(&ret, l_name);
        tmp = PyObject_Repr(self->name);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->help != 0) {
        CPY_STRCAT(&ret, l_help);
        tmp = PyObject_Repr(self->help);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->unit != 0) {
        CPY_STRCAT(&ret, l_unit);
        tmp = PyObject_Repr(self->unit);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if ((self->metrics != NULL) &&
        (!PyList_Check(self->metrics) || PySequence_Length(self->metrics) > 0)) {
        CPY_STRCAT(&ret, l_metrics);
        tmp = PyObject_Repr(self->metrics);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricFamily_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricFamily *n = (MetricFamily *)self;
    Py_VISIT(n->name);
    Py_VISIT(n->help);
    Py_VISIT(n->unit);
    Py_VISIT(n->metrics);
    return 0;
}

static int MetricFamily_clear(PyObject *self)
{
    MetricFamily *n = (MetricFamily *)self;
    Py_CLEAR(n->name);
    Py_CLEAR(n->help);
    Py_CLEAR(n->unit);
    Py_CLEAR(n->metrics);
    return 0;
}

static void MetricFamily_dealloc(PyObject *self)
{
    MetricFamily_clear(self);
    self->ob_type->tp_free(self);
}

static PyMethodDef MetricFamily_methods[] = {
    {"dispatch", (PyCFunction)MetricFamily_dispatch, METH_VARARGS | METH_KEYWORDS, fam_dispatch_doc},
    {"append", (PyCFunction)MetricFamily_append, METH_VARARGS | METH_KEYWORDS, fam_append_doc},
    {NULL}
};

static PyMemberDef MetricFamily_members[] = {
    {"name",        T_OBJECT,    offsetof(MetricFamily, name),    0, fam_name_doc   },
    {"help",        T_OBJECT,    offsetof(MetricFamily, help),    0, fam_help_doc   },
    {"unit",        T_OBJECT,    offsetof(MetricFamily, unit),    0, fam_unit_doc   },
    {"metrics",     T_OBJECT_EX, offsetof(MetricFamily, metrics), 0, fam_metrics_doc},
    {NULL}
};

PyTypeObject MetricFamilyType = {
    CPY_INIT_TYPE "ncollectd.MetricFamily", /* tp_name */
    sizeof(MetricFamily),                   /* tp_basicsize */
    0,                                      /* Will be filled in later */
    MetricFamily_dealloc,                   /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    MetricFamily_repr,                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricFamily_doc,                       /* tp_doc */
    MetricFamily_traverse,                  /* tp_traverse */
    MetricFamily_clear,                     /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    MetricFamily_methods,                   /* tp_methods */
    MetricFamily_members,                   /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    MetricFamily_init,                      /* tp_init */
    0,                                      /* tp_alloc */
    MetricFamily_new                        /* tp_new */
};
