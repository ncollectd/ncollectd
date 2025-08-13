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

static char Metric_time_doc[] =
    "This is the Unix timestamp of the time this metric was read.\n"
    "For dispatching values this can be set to 0 which means \"now\".\n"
    "This means the time the metric is actually dispatched, not the time\n"
    "it was set to 0.";

static char Metric_interval_doc[] =
    "The interval is the timespan in seconds between two submits for\n"
    "the same metric. This value has to be a positive float.\n"
    "If this member is set to a non-positive value, the default value \n"
    "as specified in the config file will be used (default: 10).";

static char Metric_labels_doc[] =
    "These are the labels for the metric object.\n"
    "It has to be a dictionary of numbers, strings or bools. All keys must be\n"
    "strings.";

static char Metric_doc[] =
    "The Metric class is a wrapper around the ncollectd metric_t.";

static int Metric_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    Metric *self = (Metric *)s;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "|Odd", kwlist,
                                             &labels, &time, &interval) ;
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->time = time;
    self->interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
        PyObject *old_labels = self->labels;
        Py_INCREF(labels);
        self->labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *Metric_new(PyTypeObject *type, __attribute__((unused)) PyObject *args,
                                  __attribute__((unused)) PyObject *kwds)
{
    Metric *self = (Metric *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->time = 0;
    self->interval = 0;
    self->labels = PyDict_New();
    return (PyObject *)self;
}

static PyObject *Metric_repr(PyObject *s)
{
    static PyObject *l_closing;

    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if (l_closing == NULL)
        return NULL;

    PyObject *ret = cpy_metric_repr(s);
    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int Metric_traverse(PyObject *self, visitproc visit, void *arg)
{
    Metric *m = (Metric *)self;
    Py_VISIT(m->labels);
    return 0;
}

static int Metric_clear(PyObject *self)
{
    Metric *m = (Metric *)self;
    Py_CLEAR(m->labels);
    return 0;
}

static void Metric_dealloc(PyObject *self)
{
    Metric_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef Metric_members[] = {
    {"time",     T_DOUBLE,    offsetof(Metric, time),     0, Metric_time_doc    },
    {"interval", T_DOUBLE,    offsetof(Metric, interval), 0, Metric_interval_doc},
    {"labels",   T_OBJECT_EX, offsetof(Metric, labels),   0, Metric_labels_doc  },
    {NULL}
};

PyTypeObject MetricType = {
    CPY_INIT_TYPE "ncollectd.Metric", /* tp_name */
    sizeof(Metric),                   /* tp_basicsize */
    0,                                /* Will be filled in later */
    Metric_dealloc,                   /* tp_dealloc */
    0,                                /* tp_print */
    0,                                /* tp_getattr */
    0,                                /* tp_setattr */
    0,                                /* tp_compare */
    Metric_repr,                      /* tp_repr */
    0,                                /* tp_as_number */
    0,                                /* tp_as_sequence */
    0,                                /* tp_as_mapping */
    0,                                /* tp_hash */
    0,                                /* tp_call */
    0,                                /* tp_str */
    0,                                /* tp_getattro */
    0,                                /* tp_setattro */
    0,                                /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    Metric_doc,                       /* tp_doc */
    Metric_traverse,                  /* tp_traverse */
    Metric_clear,                     /* tp_clear */
    0,                                /* tp_richcompare */
    0,                                /* tp_weaklistoffset */
    0,                                /* tp_iter */
    0,                                /* tp_iternext */
    0,                                /* tp_methods */
    Metric_members,                   /* tp_members */
    0,                                /* tp_getset */
    0,                                /* tp_base */
    0,                                /* tp_dict */
    0,                                /* tp_descr_get */
    0,                                /* tp_descr_set */
    0,                                /* tp_dictoffset */
    Metric_init,                      /* tp_init */
    0,                                /* tp_alloc */
    Metric_new                        /* tp_new */
};

static char MetricUnknownDouble_value_doc[] =
    "The double value for metric type unknown.";

static char MetricUnknownDouble_doc[] =
    "The MetricUnknownDouble class is a wrapper around the ncollectd unknown_t value.";

static int MetricUnknownDouble_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricUnknownDouble *self = (MetricUnknownDouble *)s;
    double value = 0;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"value", "labels", "time", "interval", NULL};
    int status = PyArg_ParseTupleAndKeywords(args, kwds, "d|Odd", kwlist,
                                             &value, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricUnknownDouble_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricUnknownDouble *self = (MetricUnknownDouble *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->value = 0;
    return (PyObject *)self;
}

static PyObject *MetricUnknownDouble_repr(PyObject *s)
{
    static PyObject *l_value, *l_closing;

    if (l_value == NULL)
        l_value = cpy_string_to_unicode_or_bytes(",value=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_value == NULL) || (l_closing == NULL))
        return NULL;

    MetricUnknownDouble *self = (MetricUnknownDouble *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_value);
    PyObject *tmp = PyFloat_FromDouble(self->value);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricUnknownDouble_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricUnknownDouble *m = (MetricUnknownDouble *)self;
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricUnknownDouble_clear(PyObject *self)
{
    MetricUnknownDouble *m = (MetricUnknownDouble *)self;
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricUnknownDouble_dealloc(PyObject *self)
{
    MetricUnknownDouble_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricUnknownDouble_members[] = {
    {"value", T_DOUBLE, offsetof(MetricUnknownDouble, value), 0, MetricUnknownDouble_value_doc},
    {NULL}
};

PyTypeObject MetricUnknownDoubleType = {
    CPY_INIT_TYPE "ncollectd.MetricUnknownDouble", /* tp_name */
    sizeof(MetricUnknownDouble),                   /* tp_basicsize */
    0,                                             /* Will be filled in later */
    MetricUnknownDouble_dealloc,                   /* tp_dealloc */
    0,                                             /* tp_print */
    0,                                             /* tp_getattr */
    0,                                             /* tp_setattr */
    0,                                             /* tp_compare */
    MetricUnknownDouble_repr,                      /* tp_repr */
    0,                                             /* tp_as_number */
    0,                                             /* tp_as_sequence */
    0,                                             /* tp_as_mapping */
    0,                                             /* tp_hash */
    0,                                             /* tp_call */
    0,                                             /* tp_str */
    0,                                             /* tp_getattro */
    0,                                             /* tp_setattro */
    0,                                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricUnknownDouble_doc,                       /* tp_doc */
    MetricUnknownDouble_traverse,                  /* tp_traverse */
    MetricUnknownDouble_clear,                     /* tp_clear */
    0,                                             /* tp_richcompare */
    0,                                             /* tp_weaklistoffset */
    0,                                             /* tp_iter */
    0,                                             /* tp_iternext */
    0,                                             /* tp_methods */
    MetricUnknownDouble_members,                   /* tp_members */
    0,                                             /* tp_getset */
    0,                                             /* tp_base */
    0,                                             /* tp_dict */
    0,                                             /* tp_descr_get */
    0,                                             /* tp_descr_set */
    0,                                             /* tp_dictoffset */
    MetricUnknownDouble_init,                      /* tp_init */
    0,                                             /* tp_alloc */
    MetricUnknownDouble_new                        /* tp_new */
};

static char MetricUnknownLong_value_doc[] =
    "The int value for metric type unknown.";

static char MetricUnknownLong_doc[] =
    "The MetricUnknownLong class is a wrapper around the ncollectd unknown metric.";

static int MetricUnknownLong_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricUnknownLong *self = (MetricUnknownLong *)s;
    long long value = 0;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"value", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "L|Odd", kwlist,
                                             &value, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricUnknownLong_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricUnknownLong *self = (MetricUnknownLong *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->value = 0;
    return (PyObject *)self;
}

static PyObject *MetricUnknownLong_repr(PyObject *s)
{
    static PyObject *l_value, *l_closing;

    if (l_value == NULL)
        l_value = cpy_string_to_unicode_or_bytes(",value=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_value == NULL) || (l_closing == NULL))
        return NULL;

    MetricUnknownLong *self = (MetricUnknownLong *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_value);
    PyObject *tmp =  PyLong_FromLongLong(self->value);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricUnknownLong_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricUnknownLong *m = (MetricUnknownLong *)self;
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricUnknownLong_clear(PyObject *self)
{
    MetricUnknownLong *m = (MetricUnknownLong *)self;
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricUnknownLong_dealloc(PyObject *self)
{
    MetricUnknownLong_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricUnknownLong_members[] = {
    {"value", T_LONG, offsetof(MetricUnknownLong, value), 0, MetricUnknownLong_value_doc},
    {NULL}
};

PyTypeObject MetricUnknownLongType = {
    CPY_INIT_TYPE "ncollectd.MetricUnknownLong", /* tp_name */
    sizeof(MetricUnknownLong),                   /* tp_basicsize */
    0,                                           /* Will be filled in later */
    MetricUnknownLong_dealloc,                   /* tp_dealloc */
    0,                                           /* tp_print */
    0,                                           /* tp_getattr */
    0,                                           /* tp_setattr */
    0,                                           /* tp_compare */
    MetricUnknownLong_repr,                      /* tp_repr */
    0,                                           /* tp_as_number */
    0,                                           /* tp_as_sequence */
    0,                                           /* tp_as_mapping */
    0,                                           /* tp_hash */
    0,                                           /* tp_call */
    0,                                           /* tp_str */
    0,                                           /* tp_getattro */
    0,                                           /* tp_setattro */
    0,                                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricUnknownLong_doc,                       /* tp_doc */
    MetricUnknownLong_traverse,                  /* tp_traverse */
    MetricUnknownLong_clear,                     /* tp_clear */
    0,                                           /* tp_richcompare */
    0,                                           /* tp_weaklistoffset */
    0,                                           /* tp_iter */
    0,                                           /* tp_iternext */
    0,                                           /* tp_methods */
    MetricUnknownLong_members,                   /* tp_members */
    0,                                           /* tp_getset */
    0,                                           /* tp_base */
    0,                                           /* tp_dict */
    0,                                           /* tp_descr_get */
    0,                                           /* tp_descr_set */
    0,                                           /* tp_dictoffset */
    MetricUnknownLong_init,                      /* tp_init */
    0,                                           /* tp_alloc */
    MetricUnknownLong_new                        /* tp_new */
};

static char MetricGaugeDouble_value_doc[] =
    "The double value for metric type gauge.";

static char MetricGaugeDouble_doc[] =
    "The MetricGaugeDouble class is a wrapper around the ncollectd gauge metric.";

static int MetricGaugeDouble_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricGaugeDouble *self = (MetricGaugeDouble *)s;
    double value = 0;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"value", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "d|Odd", kwlist,
                                             &value, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricGaugeDouble_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricGaugeDouble *self = (MetricGaugeDouble *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->value = 0;
    return (PyObject *)self;
}

static PyObject *MetricGaugeDouble_repr(PyObject *s)
{
    static PyObject *l_value, *l_closing;

    if (l_value == NULL)
        l_value = cpy_string_to_unicode_or_bytes(",value=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_value == NULL) || (l_closing == NULL))
        return NULL;

    MetricGaugeDouble *self = (MetricGaugeDouble *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_value);
    PyObject *tmp = PyFloat_FromDouble(self->value);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricGaugeDouble_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricGaugeDouble *m = (MetricGaugeDouble *)self;
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricGaugeDouble_clear(PyObject *self)
{
    MetricGaugeDouble *m = (MetricGaugeDouble *)self;
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricGaugeDouble_dealloc(PyObject *self)
{
    MetricGaugeDouble_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricGaugeDouble_members[] = {
    {"value", T_DOUBLE, offsetof(MetricGaugeDouble, value), 0, MetricGaugeDouble_value_doc},
    {NULL}
};

PyTypeObject MetricGaugeDoubleType = {
    CPY_INIT_TYPE "ncollectd.MetricGaugeDouble", /* tp_name */
    sizeof(MetricGaugeDouble),                   /* tp_basicsize */
    0,                                           /* Will be filled in later */
    MetricGaugeDouble_dealloc,                   /* tp_dealloc */
    0,                                           /* tp_print */
    0,                                           /* tp_getattr */
    0,                                           /* tp_setattr */
    0,                                           /* tp_compare */
    MetricGaugeDouble_repr,                      /* tp_repr */
    0,                                           /* tp_as_number */
    0,                                           /* tp_as_sequence */
    0,                                           /* tp_as_mapping */
    0,                                           /* tp_hash */
    0,                                           /* tp_call */
    0,                                           /* tp_str */
    0,                                           /* tp_getattro */
    0,                                           /* tp_setattro */
    0,                                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricGaugeDouble_doc,                       /* tp_doc */
    MetricGaugeDouble_traverse,                  /* tp_traverse */
    MetricGaugeDouble_clear,                     /* tp_clear */
    0,                                           /* tp_richcompare */
    0,                                           /* tp_weaklistoffset */
    0,                                           /* tp_iter */
    0,                                           /* tp_iternext */
    0,                                           /* tp_methods */
    MetricGaugeDouble_members,                   /* tp_members */
    0,                                           /* tp_getset */
    0,                                           /* tp_base */
    0,                                           /* tp_dict */
    0,                                           /* tp_descr_get */
    0,                                           /* tp_descr_set */
    0,                                           /* tp_dictoffset */
    MetricGaugeDouble_init,                      /* tp_init */
    0,                                           /* tp_alloc */
    MetricGaugeDouble_new                        /* tp_new */
};

static char MetricGaugeLong_value_doc[] =
    "The int value for metric type gauge.";

static char MetricGaugeLong_doc[] =
    "The MetricGaugeLong class is a wrapper around the ncollectd gauge_t metric.";

static int MetricGaugeLong_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricGaugeLong *self = (MetricGaugeLong *)s;
    long long value = 0;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"value", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "L|Odd", kwlist,
                                             &value, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricGaugeLong_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricGaugeLong *self = (MetricGaugeLong *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->value = 0;
    return (PyObject *)self;
}

static PyObject *MetricGaugeLong_repr(PyObject *s)
{
    static PyObject *l_value, *l_closing;

    if (l_value == NULL)
        l_value = cpy_string_to_unicode_or_bytes(",value=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_value == NULL) || (l_closing == NULL))
        return NULL;

    MetricGaugeLong *self = (MetricGaugeLong *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_value);
    PyObject *tmp =  PyLong_FromLongLong(self->value);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricGaugeLong_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricGaugeLong *m = (MetricGaugeLong *)self;
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricGaugeLong_clear(PyObject *self)
{
    MetricGaugeLong *m = (MetricGaugeLong *)self;
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricGaugeLong_dealloc(PyObject *self)
{
    MetricGaugeLong_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricGaugeLong_members[] = {
    {"value", T_LONG, offsetof(MetricGaugeLong, value), 0, MetricGaugeLong_value_doc},
    {NULL}
};

PyTypeObject MetricGaugeLongType = {
    CPY_INIT_TYPE "ncollectd.MetricGaugeLong", /* tp_name */
    sizeof(MetricGaugeLong),                   /* tp_basicsize */
    0,                                         /* Will be filled in later */
    MetricGaugeLong_dealloc,                   /* tp_dealloc */
    0,                                         /* tp_print */
    0,                                         /* tp_getattr */
    0,                                         /* tp_setattr */
    0,                                         /* tp_compare */
    MetricGaugeLong_repr,                      /* tp_repr */
    0,                                         /* tp_as_number */
    0,                                         /* tp_as_sequence */
    0,                                         /* tp_as_mapping */
    0,                                         /* tp_hash */
    0,                                         /* tp_call */
    0,                                         /* tp_str */
    0,                                         /* tp_getattro */
    0,                                         /* tp_setattro */
    0,                                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricGaugeLong_doc,                       /* tp_doc */
    MetricGaugeLong_traverse,                  /* tp_traverse */
    MetricGaugeLong_clear,                     /* tp_clear */
    0,                                         /* tp_richcompare */
    0,                                         /* tp_weaklistoffset */
    0,                                         /* tp_iter */
    0,                                         /* tp_iternext */
    0,                                         /* tp_methods */
    MetricGaugeLong_members,                   /* tp_members */
    0,                                         /* tp_getset */
    0,                                         /* tp_base */
    0,                                         /* tp_dict */
    0,                                         /* tp_descr_get */
    0,                                         /* tp_descr_set */
    0,                                         /* tp_dictoffset */
    MetricGaugeLong_init,                      /* tp_init */
    0,                                         /* tp_alloc */
    MetricGaugeLong_new                        /* tp_new */
};

static char MetricCounterDouble_value_doc[] =
    "The double value for metric type counter.";

static char MetricCounterDouble_doc[] =
    "The MetricCounterDouble class is a wrapper around the ncollectd counter_t value.";

static int MetricCounterDouble_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricCounterDouble *self = (MetricCounterDouble *)s;
    double value = 0;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"value", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "d|Odd", kwlist,
                                             &value, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricCounterDouble_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricCounterDouble *self = (MetricCounterDouble *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->value = 0;
    return (PyObject *)self;
}

static PyObject *MetricCounterDouble_repr(PyObject *s)
{
    static PyObject *l_value, *l_closing;

    if (l_value == NULL)
        l_value = cpy_string_to_unicode_or_bytes(",value=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_value == NULL) || (l_closing == NULL))
        return NULL;

    MetricCounterDouble *self = (MetricCounterDouble *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_value);
    PyObject *tmp = PyFloat_FromDouble(self->value);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricCounterDouble_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricCounterDouble *m = (MetricCounterDouble *)self;
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricCounterDouble_clear(PyObject *self)
{
    MetricCounterDouble *m = (MetricCounterDouble *)self;
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricCounterDouble_dealloc(PyObject *self)
{
    MetricCounterDouble_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricCounterDouble_members[] = {
    {"value", T_DOUBLE, offsetof(MetricCounterDouble, value), 0, MetricCounterDouble_value_doc},
    {NULL}
};

PyTypeObject MetricCounterDoubleType = {
    CPY_INIT_TYPE "ncollectd.MetricCounterDouble", /* tp_name */
    sizeof(MetricCounterDouble),                   /* tp_basicsize */
    0,                                             /* Will be filled in later */
    MetricCounterDouble_dealloc,                   /* tp_dealloc */
    0,                                             /* tp_print */
    0,                                             /* tp_getattr */
    0,                                             /* tp_setattr */
    0,                                             /* tp_compare */
    MetricCounterDouble_repr,                      /* tp_repr */
    0,                                             /* tp_as_number */
    0,                                             /* tp_as_sequence */
    0,                                             /* tp_as_mapping */
    0,                                             /* tp_hash */
    0,                                             /* tp_call */
    0,                                             /* tp_str */
    0,                                             /* tp_getattro */
    0,                                             /* tp_setattro */
    0,                                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricCounterDouble_doc,                       /* tp_doc */
    MetricCounterDouble_traverse,                  /* tp_traverse */
    MetricCounterDouble_clear,                     /* tp_clear */
    0,                                             /* tp_richcompare */
    0,                                             /* tp_weaklistoffset */
    0,                                             /* tp_iter */
    0,                                             /* tp_iternext */
    0,                                             /* tp_methods */
    MetricCounterDouble_members,                   /* tp_members */
    0,                                             /* tp_getset */
    0,                                             /* tp_base */
    0,                                             /* tp_dict */
    0,                                             /* tp_descr_get */
    0,                                             /* tp_descr_set */
    0,                                             /* tp_dictoffset */
    MetricCounterDouble_init,                      /* tp_init */
    0,                                             /* tp_alloc */
    MetricCounterDouble_new                        /* tp_new */
};

static char MetricCounterULong_value_doc[] =
    "The unsigned int value for metric type counter.";

static char MetricCounterULong_doc[] =
    "The MetricCounterULong class is a wrapper around the ncollectd counter metric.";

static int MetricCounterULong_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricCounterULong *self = (MetricCounterULong *)s;
    unsigned long long value = 0;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"value", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "K|Odd", kwlist,
                                             &value, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricCounterULong_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricCounterULong *self = (MetricCounterULong *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->value = 0;
    return (PyObject *)self;
}

static PyObject *MetricCounterULong_repr(PyObject *s)
{
    static PyObject *l_value, *l_closing;

    if (l_value == NULL)
        l_value = cpy_string_to_unicode_or_bytes(",value=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_value == NULL) || (l_closing == NULL))
        return NULL;

    MetricCounterULong *self = (MetricCounterULong *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_value);
    PyObject *tmp =  PyLong_FromUnsignedLongLong(self->value);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricCounterULong_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricCounterULong *m = (MetricCounterULong *)self;
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricCounterULong_clear(PyObject *self)
{
    MetricCounterULong *m = (MetricCounterULong *)self;
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricCounterULong_dealloc(PyObject *self)
{
    MetricCounterULong_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricCounterULong_members[] = {
    {"value", T_ULONG, offsetof(MetricCounterULong, value), 0, MetricCounterULong_value_doc},
    {NULL}
};

PyTypeObject MetricCounterULongType = {
    CPY_INIT_TYPE "ncollectd.MetricCounterULong", /* tp_name */
    sizeof(MetricCounterULong),                   /* tp_basicsize */
    0,                                            /* Will be filled in later */
    MetricCounterULong_dealloc,                   /* tp_dealloc */
    0,                                            /* tp_print */
    0,                                            /* tp_getattr */
    0,                                            /* tp_setattr */
    0,                                            /* tp_compare */
    MetricCounterULong_repr,                      /* tp_repr */
    0,                                            /* tp_as_number */
    0,                                            /* tp_as_sequence */
    0,                                            /* tp_as_mapping */
    0,                                            /* tp_hash */
    0,                                            /* tp_call */
    0,                                            /* tp_str */
    0,                                            /* tp_getattro */
    0,                                            /* tp_setattro */
    0,                                            /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricCounterULong_doc,                       /* tp_doc */
    MetricCounterULong_traverse,                  /* tp_traverse */
    MetricCounterULong_clear,                     /* tp_clear */
    0,                                            /* tp_richcompare */
    0,                                            /* tp_weaklistoffset */
    0,                                            /* tp_iter */
    0,                                            /* tp_iternext */
    0,                                            /* tp_methods */
    MetricCounterULong_members,                   /* tp_members */
    0,                                            /* tp_getset */
    0,                                            /* tp_base */
    0,                                            /* tp_dict */
    0,                                            /* tp_descr_get */
    0,                                            /* tp_descr_set */
    0,                                            /* tp_dictoffset */
    MetricCounterULong_init,                      /* tp_init */
    0,                                            /* tp_alloc */
    MetricCounterULong_new                        /* tp_new */
};

static char MetricInfo_info_doc[] =
    "These are the labels for the info metric.\n"
    "It has to be a dictionary of numbers, strings or bools. All keys must be\n"
    "strings.";

static char MetricInfo_doc[] =
    "The MetricInfo class is a wrapper around the ncollectd info metric.";

static int MetricInfo_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricInfo *self = (MetricInfo *)s;
    PyObject *info = NULL;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"info", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "O|Odd", kwlist,
                                             &info, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((info != NULL) && !PyDict_Check(info)) {
        PyErr_SetString(PyExc_TypeError, "info must be a dict");
        Py_XDECREF(info);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(info);
        Py_XDECREF(labels);
        return -1;
    }

    self->metric.time = time;
    self->metric.interval = interval;

    if (info == NULL) {
        info = PyDict_New();
        PyErr_Clear();
    }

    if (info != NULL) {
        PyObject *old_info= self->info;
        Py_INCREF(info);
        self->info = info;
        Py_XDECREF(old_info);
    }

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricInfo_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricInfo *self = (MetricInfo *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->info = PyDict_New();
    return (PyObject *)self;
}

static PyObject *MetricInfo_repr(PyObject *s)
{
    static PyObject *l_info, *l_closing;

    if (l_info == NULL)
        l_info = cpy_string_to_unicode_or_bytes(",info=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_info == NULL) || (l_closing == NULL))
        return NULL;

    MetricInfo *self = (MetricInfo *)s;

    PyObject *ret = cpy_metric_repr(s);

    if (self->info && (!PyDict_Check(self->info) || PyDict_Size(self->info) > 0)) {
        CPY_STRCAT(&ret, l_info);
        PyObject *tmp = PyObject_Repr(self->info);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricInfo_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricInfo *m = (MetricInfo *)self;
    Py_VISIT(m->info);
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricInfo_clear(PyObject *self)
{
    MetricInfo *m = (MetricInfo *)self;
    Py_CLEAR(m->info);
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricInfo_dealloc(PyObject *self)
{
    MetricInfo_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricInfo_members[] = {
    {"info", T_OBJECT_EX, offsetof(MetricInfo, info), 0, MetricInfo_info_doc},
    {NULL}
};

PyTypeObject MetricInfoType = {
    CPY_INIT_TYPE "ncollectd.MetricInfo", /* tp_name */
    sizeof(MetricInfo),                   /* tp_basicsize */
    0,                                    /* Will be filled in later */
    MetricInfo_dealloc,                   /* tp_dealloc */
    0,                                    /* tp_print */
    0,                                    /* tp_getattr */
    0,                                    /* tp_setattr */
    0,                                    /* tp_compare */
    MetricInfo_repr,                      /* tp_repr */
    0,                                    /* tp_as_number */
    0,                                    /* tp_as_sequence */
    0,                                    /* tp_as_mapping */
    0,                                    /* tp_hash */
    0,                                    /* tp_call */
    0,                                    /* tp_str */
    0,                                    /* tp_getattro */
    0,                                    /* tp_setattro */
    0,                                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricInfo_doc,                       /* tp_doc */
    MetricInfo_traverse,                  /* tp_traverse */
    MetricInfo_clear,                     /* tp_clear */
    0,                                    /* tp_richcompare */
    0,                                    /* tp_weaklistoffset */
    0,                                    /* tp_iter */
    0,                                    /* tp_iternext */
    0,                                    /* tp_methods */
    MetricInfo_members,                   /* tp_members */
    0,                                    /* tp_getset */
    0,                                    /* tp_base */
    0,                                    /* tp_dict */
    0,                                    /* tp_descr_get */
    0,                                    /* tp_descr_set */
    0,                                    /* tp_dictoffset */
    MetricInfo_init,                      /* tp_init */
    0,                                    /* tp_alloc */
    MetricInfo_new                        /* tp_new */
};

static char MetricStateSet_set_doc[] =
    "Represent a series of related boolean values.\n"
    "It has to be a dictionary of bools. All keys must be strings\n";

static char MetricStateSet_doc[] =
    "The MetricStateSet class is a wrapper around the ncollectd state_set metric.";

static int MetricStateSet_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricStateSet *self = (MetricStateSet *)s;
    PyObject *set = NULL;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"set", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "O|Odd", kwlist,
                                             &set, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((set != NULL) && !PyDict_Check(set)) {
        PyErr_SetString(PyExc_TypeError, "set must be a dict");
        Py_XDECREF(set);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(set);
        Py_XDECREF(labels);
        return -1;
    }

    self->metric.time = time;
    self->metric.interval = interval;

    if (set == NULL) {
        set = PyDict_New();
        PyErr_Clear();
    }

    if (set != NULL) {
        PyObject *old_set = self->set;
        Py_INCREF(set);
        self->set = set;
        Py_XDECREF(old_set);
    }

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricStateSet_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricStateSet *self = (MetricStateSet *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->set = PyDict_New();
    return (PyObject *)self;
}

static PyObject *MetricStateSet_repr(PyObject *s)
{
    static PyObject *l_set, *l_closing;

    if (l_set == NULL)
        l_set = cpy_string_to_unicode_or_bytes(",set=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_set == NULL) || (l_closing == NULL))
        return NULL;

    MetricStateSet *self = (MetricStateSet *)s;

    PyObject *ret = cpy_metric_repr(s);

    if ((self->set != NULL)  && (!PyDict_Check(self->set) || PyDict_Size(self->set) > 0)) {
        CPY_STRCAT(&ret, l_set);
        PyObject *tmp = PyObject_Repr(self->set);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricStateSet_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricStateSet *m = (MetricStateSet *)self;
    Py_VISIT(m->set);
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricStateSet_clear(PyObject *self)
{
    MetricStateSet *m = (MetricStateSet *)self;
    Py_CLEAR(m->set);
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricStateSet_dealloc(PyObject *self)
{
    MetricStateSet_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricStateSet_members[] = {
    {"set", T_OBJECT_EX, offsetof(MetricStateSet, set), 0, MetricStateSet_set_doc},
    {NULL}
};

PyTypeObject MetricStateSetType = {
    CPY_INIT_TYPE "ncollectd.MetricStateSet", /* tp_name */
    sizeof(MetricStateSet),                   /* tp_basicsize */
    0,                                        /* Will be filled in later */
    MetricStateSet_dealloc,                   /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    MetricStateSet_repr,                      /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricStateSet_doc,                       /* tp_doc */
    MetricStateSet_traverse,                  /* tp_traverse */
    MetricStateSet_clear,                     /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    MetricStateSet_members,                   /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    MetricStateSet_init,                      /* tp_init */
    0,                                        /* tp_alloc */
    MetricStateSet_new                        /* tp_new */
};

static char MetricSummary_sum_doc[] =
    "The sum of all values.";

static char MetricSummary_quantiles_doc[] =
    "Quantiles are a list of tuples of quantile and a value.";

static char MetricSummary_doc[] =
    "The MetricSummary class is a wrapper around the ncollectd summary metric.";

static int MetricSummary_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricSummary *self = (MetricSummary *)s;
    double sum = 0;
    unsigned long long count = 0;
    PyObject *quantiles = NULL;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"sum", "count", "quantiles", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "dKO|Odd", kwlist,
                                             &sum, &count, &quantiles, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((quantiles != NULL) && (!PyTuple_Check(quantiles) && !PyList_Check(quantiles))) {
        PyErr_SetString(PyExc_TypeError, "quantiles must be a list");
        Py_XDECREF(quantiles);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(quantiles);
        Py_XDECREF(labels);
        return -1;
    }

    self->metric.time = time;
    self->metric.interval = interval;
    self->sum = sum;
    self->count = count;

    if (quantiles == NULL) {
        quantiles = PyList_New(0);
        if (quantiles != NULL) {
            PyObject *old_quantiles = self->quantiles;
            self->quantiles = quantiles;
            PyErr_Clear();
            Py_XDECREF(old_quantiles);
        }
    } else {
        PyObject *old_quantiles = self->quantiles;
        Py_INCREF(quantiles);
        self->quantiles = quantiles;
        Py_XDECREF(old_quantiles);
    }

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricSummary_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricSummary *self = (MetricSummary *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->quantiles = PyList_New(0);
    return (PyObject *)self;
}

static PyObject *MetricSummary_repr(PyObject *s)
{
    static PyObject *l_sum, *l_count, *l_quantiles, *l_closing;

    if (l_sum  == NULL)
        l_sum = cpy_string_to_unicode_or_bytes(",sum=");
    if (l_count == NULL)
        l_count = cpy_string_to_unicode_or_bytes(",count=");
    if (l_quantiles == NULL)
        l_quantiles = cpy_string_to_unicode_or_bytes(",quantiles=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_sum == NULL) || (l_count == NULL) || (l_quantiles == NULL) || (l_closing == NULL))
        return NULL;

    MetricSummary *self = (MetricSummary *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_sum);
    PyObject *tmp = PyFloat_FromDouble(self->sum);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    CPY_STRCAT(&ret, l_count);
    tmp =  PyLong_FromLongLong(self->count);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    if (self->quantiles != NULL) {
        CPY_STRCAT(&ret, l_quantiles);
        tmp = PyObject_Repr(self->quantiles);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricSummary_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricSummary *m = (MetricSummary *)self;
    Py_VISIT(m->quantiles);
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricSummary_clear(PyObject *self)
{
    MetricSummary *m = (MetricSummary *)self;
    Py_CLEAR(m->quantiles);
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricSummary_dealloc(PyObject *self)
{
    MetricSummary_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricSummary_members[] = {
    {"sum", T_DOUBLE, offsetof(MetricSummary, sum), 0, MetricSummary_sum_doc},
    {"quantiles", T_OBJECT_EX, offsetof(MetricSummary, quantiles), 0, MetricSummary_quantiles_doc},
    {NULL}
};

PyTypeObject MetricSummaryType = {
    CPY_INIT_TYPE "ncollectd.MetricSummary", /* tp_name */
    sizeof(MetricSummary),                   /* tp_basicsize */
    0,                                       /* Will be filled in later */
    MetricSummary_dealloc,                   /* tp_dealloc */
    0,                                       /* tp_print */
    0,                                       /* tp_getattr */
    0,                                       /* tp_setattr */
    0,                                       /* tp_compare */
    MetricSummary_repr,                      /* tp_repr */
    0,                                       /* tp_as_number */
    0,                                       /* tp_as_sequence */
    0,                                       /* tp_as_mapping */
    0,                                       /* tp_hash */
    0,                                       /* tp_call */
    0,                                       /* tp_str */
    0,                                       /* tp_getattro */
    0,                                       /* tp_setattro */
    0,                                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricSummary_doc,                       /* tp_doc */
    MetricSummary_traverse,                  /* tp_traverse */
    MetricSummary_clear,                     /* tp_clear */
    0,                                       /* tp_richcompare */
    0,                                       /* tp_weaklistoffset */
    0,                                       /* tp_iter */
    0,                                       /* tp_iternext */
    0,                                       /* tp_methods */
    MetricSummary_members,                   /* tp_members */
    0,                                       /* tp_getset */
    0,                                       /* tp_base */
    0,                                       /* tp_dict */
    0,                                       /* tp_descr_get */
    0,                                       /* tp_descr_set */
    0,                                       /* tp_dictoffset */
    MetricSummary_init,                      /* tp_init */
    0,                                       /* tp_alloc */
    MetricSummary_new                        /* tp_new */
};

static char MetricHistogram_sum_doc[] =
    "The sum of all values.";

static char MetricHistogram_buckets_doc[] =
    "Buckets are a list of tuples, each tuple is a bucket covers the values\n"
    "less and or equal to it, it has two items: the counter and the maximum value.\n";

static char MetricHistogram_doc[] =
    "The MetricHistogram class is a wrapper around the ncollectd histogram metric.";

static int MetricHistogram_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricHistogram *self = (MetricHistogram *)s;
    double sum = 0;
    PyObject *buckets = NULL;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"sum", "buckets", "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "dO|Odd", kwlist,
                                             &sum, &buckets, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((buckets!= NULL) && (!PyTuple_Check(buckets) && !PyList_Check(buckets))) {
        PyErr_SetString(PyExc_TypeError, "set must be a list");
        Py_XDECREF(buckets);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(buckets);
        Py_XDECREF(labels);
        return -1;
    }

    self->metric.time = time;
    self->metric.interval = interval;
    self->sum = sum;

    if (buckets == NULL) {
        buckets = PyList_New(0);
        if (buckets != NULL) {
            PyObject *old_buckets = self->buckets;
            self->buckets = buckets;
            PyErr_Clear();
            Py_XDECREF(old_buckets);
        }
    } else {
        PyObject *old_buckets = self->buckets;
        Py_INCREF(buckets);
        self->buckets = buckets;
        Py_XDECREF(old_buckets);
    }

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricHistogram_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricHistogram *self = (MetricHistogram *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->buckets = PyList_New(0);

    return (PyObject *)self;
}

static PyObject *MetricHistogram_repr(PyObject *s)
{
    static PyObject *l_sum, *l_buckets, *l_closing;

    if (l_sum  == NULL)
        l_sum = cpy_string_to_unicode_or_bytes(",sum=");
    if (l_buckets  == NULL)
        l_buckets = cpy_string_to_unicode_or_bytes(",buckets=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_sum == NULL) || (l_buckets  == NULL) || (l_closing == NULL))
        return NULL;

    MetricHistogram *self = (MetricHistogram *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_sum);
    PyObject *tmp = PyFloat_FromDouble(self->sum);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    if (self->buckets != NULL) {
        CPY_STRCAT(&ret, l_buckets);
        tmp = PyObject_Repr(self->buckets);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricHistogram_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricHistogram *m = (MetricHistogram *)self;
    Py_VISIT(m->buckets);
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricHistogram_clear(PyObject *self)
{
    MetricHistogram *m = (MetricHistogram *)self;
    Py_CLEAR(m->buckets);
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricHistogram_dealloc(PyObject *self)
{
    MetricHistogram_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricHistogram_members[] = {
    {"sum", T_DOUBLE, offsetof(MetricHistogram, sum), 0, MetricHistogram_sum_doc},
    {"buckets", T_OBJECT_EX, offsetof(MetricHistogram, buckets), 0, MetricHistogram_buckets_doc},
    {NULL}
};

PyTypeObject MetricHistogramType = {
    CPY_INIT_TYPE "ncollectd.MetricHistogram", /* tp_name */
    sizeof(MetricHistogram),                   /* tp_basicsize */
    0,                                         /* Will be filled in later */
    MetricHistogram_dealloc,                   /* tp_dealloc */
    0,                                         /* tp_print */
    0,                                         /* tp_getattr */
    0,                                         /* tp_setattr */
    0,                                         /* tp_compare */
    MetricHistogram_repr,                      /* tp_repr */
    0,                                         /* tp_as_number */
    0,                                         /* tp_as_sequence */
    0,                                         /* tp_as_mapping */
    0,                                         /* tp_hash */
    0,                                         /* tp_call */
    0,                                         /* tp_str */
    0,                                         /* tp_getattro */
    0,                                         /* tp_setattro */
    0,                                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricHistogram_doc,                       /* tp_doc */
    MetricHistogram_traverse,                  /* tp_traverse */
    MetricHistogram_clear,                     /* tp_clear */
    0,                                         /* tp_richcompare */
    0,                                         /* tp_weaklistoffset */
    0,                                         /* tp_iter */
    0,                                         /* tp_iternext */
    0,                                         /* tp_methods */
    MetricHistogram_members,                   /* tp_members */
    0,                                         /* tp_getset */
    0,                                         /* tp_base */
    0,                                         /* tp_dict */
    0,                                         /* tp_descr_get */
    0,                                         /* tp_descr_set */
    0,                                         /* tp_dictoffset */
    MetricHistogram_init,                      /* tp_init */
    0,                                         /* tp_alloc */
    MetricHistogram_new                        /* tp_new */
};

static char MetricGaugeHistogram_sum_doc[] =
    "The sum of all values.";

static char MetricGaugeHistogram_buckets_doc[] =
    "Buckets are a list of tuples, each tuple is a bucket covers the values\n"
    "less and or equal to it, it has two items: the counter and the maximum value.\n";

static char MetricGaugeHistogram_doc[] =
    "The MetricGaugeHistogram class is a wrapper around the ncollectd gaugehistogram metric.";

static int MetricGaugeHistogram_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    MetricGaugeHistogram *self = (MetricGaugeHistogram *)s;
    double sum = 0;
    PyObject *buckets = NULL;
    double time = 0;
    double interval = 0;
    PyObject *labels = NULL;

    static char *kwlist[] = {"sum", "buckets" "labels", "time", "interval", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "dO|Odd", kwlist,
                                             &sum, &buckets, &labels, &time, &interval);
    if (status == 0)
        return -1;

    if ((buckets!= NULL) && (!PyTuple_Check(buckets) && !PyList_Check(buckets))) {
        PyErr_SetString(PyExc_TypeError, "set must be a list");
        Py_XDECREF(buckets);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && !PyDict_Check(labels)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(buckets);
        Py_XDECREF(labels);
        return -1;
    }

    self->metric.time = time;
    self->metric.interval = interval;
    self->sum = sum;

    if (buckets == NULL) {
        buckets = PyList_New(0);
        if (buckets != NULL) {
            PyObject *old_buckets = self->buckets;
            self->buckets = buckets;
            PyErr_Clear();
            Py_XDECREF(old_buckets);
        }
    } else {
        PyObject *old_buckets = self->buckets;
        Py_INCREF(buckets);
        self->buckets = buckets;
        Py_XDECREF(old_buckets);
    }

    if (labels == NULL) {
        labels = PyDict_New();
        if (labels != NULL) {
            PyObject *old_labels = self->metric.labels;
            self->metric.labels = labels;
            PyErr_Clear();
            Py_XDECREF(old_labels);
        }
    } else {
        PyObject *old_labels = self->metric.labels;
        Py_INCREF(labels);
        self->metric.labels = labels;
        Py_XDECREF(old_labels);
    }

    return 0;
}

static PyObject *MetricGaugeHistogram_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MetricGaugeHistogram *self = (MetricGaugeHistogram *)Metric_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->buckets = PyList_New(0);

    return (PyObject *)self;
}

static PyObject *MetricGaugeHistogram_repr(PyObject *s)
{
    static PyObject *l_sum, *l_buckets, *l_closing;

    if (l_sum  == NULL)
        l_sum = cpy_string_to_unicode_or_bytes(",sum=");
    if (l_buckets  == NULL)
        l_buckets = cpy_string_to_unicode_or_bytes(",buckets=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_sum == NULL) || (l_buckets  == NULL) || (l_closing == NULL))
        return NULL;

    MetricGaugeHistogram *self = (MetricGaugeHistogram *)s;

    PyObject *ret = cpy_metric_repr(s);

    CPY_STRCAT(&ret, l_sum);
    PyObject *tmp = PyFloat_FromDouble(self->sum);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    if (self->buckets != NULL) {
        CPY_STRCAT(&ret, l_buckets);
        tmp = PyObject_Repr(self->buckets);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int MetricGaugeHistogram_traverse(PyObject *self, visitproc visit, void *arg)
{
    MetricGaugeHistogram *m = (MetricGaugeHistogram *)self;
    Py_VISIT(m->buckets);
    Py_VISIT(m->metric.labels);
    return 0;
}

static int MetricGaugeHistogram_clear(PyObject *self)
{
    MetricGaugeHistogram *m = (MetricGaugeHistogram *)self;
    Py_CLEAR(m->buckets);
    Py_CLEAR(m->metric.labels);
    return 0;
}

static void MetricGaugeHistogram_dealloc(PyObject *self)
{
    MetricGaugeHistogram_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef MetricGaugeHistogram_members[] = {
    {"sum", T_DOUBLE, offsetof(MetricGaugeHistogram, sum), 0, MetricGaugeHistogram_sum_doc},
    {"buckets", T_OBJECT_EX, offsetof(MetricGaugeHistogram, buckets), 0, MetricGaugeHistogram_buckets_doc},
    {NULL}
};

PyTypeObject MetricGaugeHistogramType = {
    CPY_INIT_TYPE "ncollectd.MetricGaugeHistogram", /* tp_name */
    sizeof(MetricGaugeHistogram),                   /* tp_basicsize */
    0,                                              /* Will be filled in later */
    MetricGaugeHistogram_dealloc,                   /* tp_dealloc */
    0,                                              /* tp_print */
    0,                                              /* tp_getattr */
    0,                                              /* tp_setattr */
    0,                                              /* tp_compare */
    MetricGaugeHistogram_repr,                      /* tp_repr */
    0,                                              /* tp_as_number */
    0,                                              /* tp_as_sequence */
    0,                                              /* tp_as_mapping */
    0,                                              /* tp_hash */
    0,                                              /* tp_call */
    0,                                              /* tp_str */
    0,                                              /* tp_getattro */
    0,                                              /* tp_setattro */
    0,                                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    MetricGaugeHistogram_doc,                       /* tp_doc */
    MetricGaugeHistogram_traverse,                  /* tp_traverse */
    MetricGaugeHistogram_clear,                     /* tp_clear */
    0,                                              /* tp_richcompare */
    0,                                              /* tp_weaklistoffset */
    0,                                              /* tp_iter */
    0,                                              /* tp_iternext */
    0,                                              /* tp_methods */
    MetricGaugeHistogram_members,                   /* tp_members */
    0,                                              /* tp_getset */
    0,                                              /* tp_base */
    0,                                              /* tp_dict */
    0,                                              /* tp_descr_get */
    0,                                              /* tp_descr_set */
    0,                                              /* tp_dictoffset */
    MetricGaugeHistogram_init,                      /* tp_init */
    0,                                              /* tp_alloc */
    MetricGaugeHistogram_new                        /* tp_new */
};
