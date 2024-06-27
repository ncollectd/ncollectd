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
static char notif_name_doc[] =
    "The name of the notification.";

static char notif_severity_doc[] =
    "The severity of this notification. Assign or compare to\n"
    "NOTIF_FAILURE, NOTIF_WARNING or NOTIF_OKAY.";

static char notif_time_doc[] =
    "This is the Unix timestamp of the time this value was read.\n"
    "For dispatching notifications this can be set to 0 which means \"now\".\n"
    "This means the time the value is actually dispatched, not the time\n"
    "it was set to 0.";

static char notif_labels_doc[] =
    "These are the labels for the Notification object.\n"
    "It has to be a dictionary of numbers, strings or bools. All keys must be\n"
    "strings.";

static char notif_annotations_doc[] =
    "These are the annotations for the Notification object.\n"
    "It has to be a dictionary of numbers, strings or bools. All keys must be\n"
    "strings.";

static char Notification_doc[] =
    "The Notification class is a wrapper around the ncollectd notification.\n"
    "It can be used to notify other plugins about bad stuff happening.\n"
    "Notifications can be dispatched at any time and can be received with "
    "register_notification.";

static char notif_dispatch_doc[] =
    "dispatch([name][, severity][, timestamp][, labels][, annotations]) "
    "-> None.  Dispatch a notification.\n"
    "\n"
    "Dispatch this notification to the ncollectd process. The object has members\n"
    "for each of the possible arguments for this method. For a detailed explanation\n"
    "of these parameters see the member of the same same.\n"
    "\n"
    "If you do not submit a parameter the value saved in its member will be submitted.\n"
    "If you do provide a parameter it will be used instead, without altering the member.";

static int Notification_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    Notification *self = (Notification *)s;
    PyObject *name = NULL;
    int severity = NOTIF_FAILURE;
    double time = 0;
    PyObject *labels = NULL;
    PyObject *annotations = NULL;

    static char *kwlist[] = {"name", "severity", "time", "labels", "annotations", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "O|idOO", kwlist,
                                             &name, &severity, &time, &labels, &annotations);
    if (status == 0)
        return -1;

    if (!IS_BYTES_OR_UNICODE(name)) {
        PyErr_SetString(PyExc_TypeError, "argument 1 must be str");
        Py_XDECREF(name);
        Py_XDECREF(labels);
        Py_XDECREF(annotations);
        return -1;
    }

    self->time = time;

    switch (severity) {
    case NOTIF_FAILURE:
    case NOTIF_WARNING:
    case NOTIF_OKAY:
        break;
    default:
        PyErr_Format(PyExc_TypeError, "Invalid severity: %d", severity);
        Py_XDECREF(name);
        Py_XDECREF(labels);
        Py_XDECREF(annotations);
        return -1;
        break;
    }
    self->severity = severity;

    PyObject *old_name = self->name;
    Py_INCREF(name);
    self->name = name;
    Py_XDECREF(old_name);

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
        PyObject *old_labels = self->labels;
        self->labels = labels;
        Py_XDECREF(old_labels);
    } else {
        PyObject *old_labels = self->labels;
        Py_INCREF(labels);
        self->labels = labels;
        Py_XDECREF(old_labels);
    }

    if (annotations == NULL) {
        annotations = PyDict_New();
        PyErr_Clear();
        PyObject *old_annotations = self->annotations;
        self->annotations = annotations;
        Py_XDECREF(old_annotations);
    } else {
        PyObject *old_annotations = self->annotations;
        Py_INCREF(annotations);
        self->annotations = annotations;
        Py_XDECREF(old_annotations);
    }

    return 0;
}

static PyObject *Notification_dispatch(Notification *self, PyObject *args, PyObject *kwds)
{
    notification_t n = {0};

    PyObject *name = NULL;
    double time = self->time;
    int severity = self->severity;
    PyObject *labels = NULL;
    PyObject *annotations = NULL;

    static char *kwlist[] = {"name", "severity", "time", "labels", "annotations", NULL};

    int status = PyArg_ParseTupleAndKeywords(args, kwds, "|OidOO", kwlist,
                                             &name, &severity, &time, &labels, &annotations);
    if (status == 0)
        return NULL;

    if (self->name == NULL) {
        if (name == NULL) {
            Py_XDECREF(labels);
            Py_XDECREF(annotations);
            PyErr_SetString(PyExc_TypeError, "missing name");
            return NULL;
        }

        if (!IS_BYTES_OR_UNICODE(name)) {
            Py_XDECREF(name);
            Py_XDECREF(labels);
            Py_XDECREF(annotations);
            PyErr_SetString(PyExc_TypeError, "name must be str");
            return NULL;
        }
    } else {
        if (!IS_BYTES_OR_UNICODE(self->name)) {
            Py_XDECREF(name);
            Py_XDECREF(labels);
            Py_XDECREF(annotations);
            PyErr_SetString(PyExc_TypeError, "name must be str");
            return NULL;
        }

    }

    switch (severity) {
    case NOTIF_FAILURE:
    case NOTIF_WARNING:
    case NOTIF_OKAY:
        break;
    default:
        PyErr_SetString(PyExc_TypeError, "invalid severity value");
        return NULL;
        break;
    }

    if ((labels!= NULL) && (labels != Py_None) && (!PyDict_Check(labels))) {
        Py_XDECREF(name);
        Py_XDECREF(labels);
        Py_XDECREF(annotations);
        PyErr_Format(PyExc_TypeError, "labels must be a dict");
        return NULL;
    }

    if ((annotations != NULL) && (annotations != Py_None) && (!PyDict_Check(annotations))) {
        Py_XDECREF(name);
        Py_XDECREF(labels);
        Py_XDECREF(annotations);
        PyErr_Format(PyExc_TypeError, "annotations must be a dict");
        return NULL;
    }

    if ((labels != NULL) && (labels != Py_None)) {
        cpy_build_labels(labels, &n.label);
    } else if ((self->labels != NULL) && (self->labels != Py_None)) {
        if (PyDict_Check(self->labels))
            cpy_build_labels(self->labels, &n.label);
    }

    if ((annotations != NULL) && (annotations != Py_None)) {
        cpy_build_labels(annotations, &n.annotation);
    } else if ((self->annotations != NULL) && (self->annotations != Py_None)) {
        if (PyDict_Check(self->annotations))
            cpy_build_labels(self->annotations, &n.annotation);
    }

    PyObject *str_name = NULL;
    if (name != NULL)
        str_name = PyObject_Str(name);
    else
        str_name = PyObject_Str(self->name);

    const char *string = cpy_unicode_or_bytes_to_string(&str_name);
    if (string == NULL) {
        Py_XDECREF(str_name);
        Py_XDECREF(name);
        Py_XDECREF(labels);
        Py_XDECREF(annotations);
        return NULL;
    }

    n.name = discard_const(string);

    n.time = DOUBLE_TO_CDTIME_T(time);
    if (n.time == 0)
        n.time = cdtime();

    n.severity = severity;

    Py_BEGIN_ALLOW_THREADS;
    status = plugin_dispatch_notification(&n);
    Py_END_ALLOW_THREADS;

    label_set_reset(&n.label);
    label_set_reset(&n.annotation);
    Py_XDECREF(str_name);
    Py_XDECREF(name);
    Py_XDECREF(labels);
    Py_XDECREF(annotations);

    if (status != 0) {
        PyErr_SetString(PyExc_RuntimeError, "error dispatching notification, read the logs");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *Notification_new(PyTypeObject *type, __attribute__((unused)) PyObject *args,
                                  __attribute__((unused)) PyObject *kwds)
{
    Notification *self = (Notification *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->name = NULL;
    self->severity = 0;
    self->time = 0;
    self->labels = PyDict_New();
    self->annotations = PyDict_New();
    return (PyObject *)self;
}

static const char *cpy_notifycation_severity(severity_t severity)
{
    switch (severity) {
    case NOTIF_FAILURE:
        return "NOTIF_FAILURE";
    case NOTIF_WARNING:
        return "NOTIF_WARNING";
    case NOTIF_OKAY:
        return "NOTIF_OKAY";
    }
    return NULL;
}

static PyObject *Notification_repr(PyObject *s)
{
    PyObject *tmp;
    static PyObject *l_open, *l_name, *l_severity, *l_time, *l_labels, *l_annotations, *l_closing;
    Notification *self = (Notification *)s;

    if (l_open == NULL)
        l_open = cpy_string_to_unicode_or_bytes("(");
    if (l_name== NULL)
        l_name = cpy_string_to_unicode_or_bytes("name=");
    if (l_severity == NULL)
        l_severity = cpy_string_to_unicode_or_bytes(",severity=");
    if (l_time == NULL)
        l_time = cpy_string_to_unicode_or_bytes(",time=");
    if (l_labels == NULL)
        l_labels = cpy_string_to_unicode_or_bytes(",labels=");
    if (l_annotations == NULL)
        l_annotations = cpy_string_to_unicode_or_bytes(",annotations=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if ((l_open == NULL) || (l_name == NULL) || (l_severity == NULL) || (l_time == NULL) ||
        (l_labels == NULL) || (l_annotations == NULL) || (l_closing == NULL))
        return NULL;

    PyObject *ret = cpy_string_to_unicode_or_bytes(s->ob_type->tp_name);

    CPY_STRCAT(&ret, l_open);

    if (self->name != 0) {
        CPY_STRCAT(&ret, l_name);
        tmp = PyObject_Repr(self->name);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_severity);
    const char *notifty_severity = cpy_notifycation_severity(self->severity);
    if (notifty_severity != NULL) {
        PyObject *severity = cpy_string_to_unicode_or_bytes(notifty_severity);
        if (severity != NULL)
            CPY_STRCAT_AND_DEL(&ret, severity);
    } else {
        tmp = PyInt_FromLong(self->severity);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->time != 0) {
        CPY_STRCAT(&ret, l_time);
        tmp = PyFloat_FromDouble(self->time);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->labels && (!PyDict_Check(self->labels) || PyDict_Size(self->labels) > 0)) {
        CPY_STRCAT(&ret, l_labels);
        tmp = PyObject_Repr(self->labels);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->annotations && (!PyDict_Check(self->annotations) || PyDict_Size(self->annotations) > 0)) {
        CPY_STRCAT(&ret, l_annotations);
        tmp = PyObject_Repr(self->annotations);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    CPY_STRCAT(&ret, l_closing);

    return ret;
}

static int Notification_traverse(PyObject *self, visitproc visit, void *arg)
{
    Notification *n = (Notification *)self;
    Py_VISIT(n->name);
    Py_VISIT(n->labels);
    Py_VISIT(n->annotations);
    return 0;
}

static int Notification_clear(PyObject *self)
{
    Notification *n = (Notification *)self;
    Py_CLEAR(n->name);
    Py_CLEAR(n->labels);
    Py_CLEAR(n->annotations);
    return 0;
}

static void Notification_dealloc(PyObject *self)
{
    Notification_clear(self);
    self->ob_type->tp_free(self);
}

static PyMethodDef Notification_methods[] = {
    {"dispatch", (PyCFunction)Notification_dispatch, METH_VARARGS | METH_KEYWORDS, notif_dispatch_doc},
    {NULL}
};

static PyMemberDef Notification_members[] = {
    {"name",        T_OBJECT,    offsetof(Notification, name),        0, notif_name_doc       },
    {"severity",    T_INT,       offsetof(Notification, severity),    0, notif_severity_doc   },
    {"time",        T_DOUBLE,    offsetof(Notification, time),        0, notif_time_doc       },
    {"labels",      T_OBJECT_EX, offsetof(Notification, labels),      0, notif_labels_doc     },
    {"annotations", T_OBJECT_EX, offsetof(Notification, annotations), 0, notif_annotations_doc},
    {NULL}
};

PyTypeObject NotificationType = {
    CPY_INIT_TYPE "ncollectd.Notification", /* tp_name */
    sizeof(Notification),                   /* tp_basicsize */
    0,                                      /* Will be filled in later */
    Notification_dealloc,                   /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    Notification_repr,                      /* tp_repr */
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
    Notification_doc,                       /* tp_doc */
    Notification_traverse,                  /* tp_traverse */
    Notification_clear,                     /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    0,                                      /* tp_iter */
    0,                                      /* tp_iternext */
    Notification_methods,                   /* tp_methods */
    Notification_members,                   /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    Notification_init,                      /* tp_init */
    0,                                      /* tp_alloc */
    Notification_new                        /* tp_new */
};
