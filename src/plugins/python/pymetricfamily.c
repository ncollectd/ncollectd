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

static char fam_name_doc[] =
    "The name of the metric family.";

static char fam_help_doc[] =
    "The name of the metric family.";

static char fam_unit_doc[] =
    "The name of the metric family.";

static char fam_metrics_doc[] = "";

static char MetricFamily_doc[] =
    "The MetricFamily class is a wrapper around the ncollectd metric_family_t.\n"
    "It can be used to submit metrics.\n"
    "MetricFamilys can be dispatched at any time and can be received with "
    "register_read.";

static char fam_dispatch_doc[] =
    "dispatch([metrics]) -> None.  Dispatch metrics.\n"
    "\n"
    "Dispatch this metrics to the ncollectd process.\n"
    "\n"
    "If you do not submit the metrics the value saved in its member will be submitted.\n"
    "If you do provide the metrics it will be used instead, "
    "without altering the metrics in the metrics family.";

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
    if (status != 0)
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

    if ((metrics != NULL) && (PyTuple_Check(metrics) == 0 && PyList_Check(metrics) == 0)) {
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

    if (metrics == NULL) {
        metrics = PyList_New(0);
        PyErr_Clear();
    }

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

    if (metrics != NULL) {
        PyObject *old_metrics = self->metrics;
        Py_INCREF(metrics);
        self->metrics = metrics;
        Py_XDECREF(old_metrics);
    }

    return 0;
}

static PyObject *MetricFamily_dispatch(MetricFamily *self, PyObject *args, PyObject *kwds)
{
    int ret;

    PyObject *metrics = self->metrics;
    double time = 0;
    static char *kwlist[] = {"metrics", "time" , NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "|Od", kwlist, &metrics, &time);
    if (status != 0)
        return NULL;

    if (self->name == NULL) {
        PyErr_SetString(PyExc_TypeError, "missing name");
        return NULL;
    }

    if (!IS_BYTES_OR_UNICODE(self->name)) {
        PyErr_SetString(PyExc_TypeError, "name must be str");
        return NULL;
    }

    if ((self->help != NULL) && !IS_BYTES_OR_UNICODE(self->help)) {
        PyErr_SetString(PyExc_TypeError, "help must be str");
        return NULL;
    }

    if ((self->unit != NULL) && !IS_BYTES_OR_UNICODE(self->unit)) {
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
        PyErr_SetString(PyExc_TypeError, "invalid severity value");
        return NULL;
        break;
    }

    metric_family_t fam = {0};

    fam.type = self->type;

    if ((metrics != NULL) && (PyTuple_Check(metrics) == 0 && PyList_Check(metrics) == 0)) {
        PyErr_Format(PyExc_TypeError, "labels must be a dict");
        return NULL;
    }

    if (self->unit != NULL) {
        PyObject *tmp = PyObject_Str(self->unit);
        const char *string = cpy_unicode_or_bytes_to_string(&tmp);
        if (string != NULL)
            fam.unit = discard_const(string);
        Py_XDECREF(tmp);
    }

    if (self->help != NULL) {
        PyObject *tmp = PyObject_Str(self->help);
        const char *string = cpy_unicode_or_bytes_to_string(&tmp);
        if (string != NULL)
            fam.help = discard_const(string);
        Py_XDECREF(tmp);
    }

    PyObject *tmp = PyObject_Str(self->name);
    const char *string = cpy_unicode_or_bytes_to_string(&tmp);
    if (string != NULL)
        fam.name = discard_const(string);
    Py_XDECREF(tmp);


    size_t size = (size_t)PySequence_Length(metrics);
    fam.metric.ptr = calloc(size, sizeof(*fam.metric.ptr));
    if (fam.metric.ptr == NULL) {
        PyErr_SetString(PyExc_TypeError, "failed to alloc metrics");
        return NULL;
    }

    for (size_t i = 0; i < size; ++i) {
        PyObject *item = PySequence_Fast_GET_ITEM(metrics, (int)i); /* Borrowed reference. */
        if (item != NULL) {

        }
#if 0
        PyObject *num;
        switch (ds->ds[i].type) {
        case DS_TYPE_COUNTER:
            num = PyNumber_Long(item); /* New reference. */
            if (num != NULL) {
                value[i].counter = PyLong_AsUnsignedLongLong(num);
                Py_XDECREF(num);
            }
            break;
        case DS_TYPE_GAUGE:
            num = PyNumber_Float(item); /* New reference. */
            if (num != NULL) {
                value[i].gauge = PyFloat_AsDouble(num);
                Py_XDECREF(num);
            }
            break;
        case DS_TYPE_DERIVE:
            /* This might overflow without raising an exception.
             * Not much we can do about it */
            num = PyNumber_Long(item); /* New reference. */
            if (num != NULL) {
                value[i].derive = PyLong_AsLongLong(num);
                Py_XDECREF(num);
            }
            break;
        default:
            free(value);
            PyErr_Format(PyExc_RuntimeError, "unknown data type %d for %s", ds->ds[i].type, value_list.type);
            return NULL;
        }
#endif
        if (PyErr_Occurred() != NULL) {
//            free(value);
            return NULL;
        }
    }
    fam.metric.num = size;

    Py_BEGIN_ALLOW_THREADS;
    ret = plugin_dispatch_metric_family(&fam, CDTIME_T_TO_DOUBLE(time));
    Py_END_ALLOW_THREADS;

    if (ret != 0) {
        PyErr_SetString(PyExc_RuntimeError, "error dispatching metric family, read the logs");
        return NULL;
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

static PyObject *MetricFamily_repr(PyObject *s)
{
    PyObject *tmp;
    static PyObject *l_open, *l_name, *l_help, *l_unit, *l_metrics, *l_closing;
    MetricFamily *self = (MetricFamily *)s;

    if (l_open == NULL)
        l_open = cpy_string_to_unicode_or_bytes("(");
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

    if ((l_name == NULL) || (l_help == NULL) || (l_unit == NULL) ||
        (l_metrics == NULL) || (l_closing == NULL))
        return NULL;

    PyObject *ret = cpy_string_to_unicode_or_bytes(s->ob_type->tp_name);

    CPY_STRCAT(&ret, l_open);

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
