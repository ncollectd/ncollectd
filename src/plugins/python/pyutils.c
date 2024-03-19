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

int cpy_build_labels(PyObject *dict, label_set_t *labels)
{
    if ((dict == NULL) || (dict == Py_None))
        return -1;

    PyObject *l = PyDict_Items(dict); /* New reference. */
    if (l == NULL) {
        cpy_log_exception("building labels");
        return -1;
    }

    int s = PyList_Size(l);
    if (s <= 0) {
        Py_XDECREF(l);
        return -1;
    }

    for (int i = 0; i < s; ++i) {
        PyObject *item = PyList_GET_ITEM(l, i);
        PyObject *key = PyTuple_GET_ITEM(item, 0);
        Py_INCREF(key);
        const char *keystring = cpy_unicode_or_bytes_to_string(&key);
        if (keystring == NULL) {
            PyErr_Clear();
            Py_XDECREF(key);
            continue;
        }

        PyObject *value = PyTuple_GET_ITEM(item, 1);
        Py_INCREF(value);
        if (value == Py_True) {
            label_set_add(labels, true, keystring, "true");
        } else if (value == Py_False) {
            label_set_add(labels, true, keystring, "false");
        } else if (PyFloat_Check(value)) {
            char buffer[DTOA_MAX];
            dtoa(PyFloat_AsDouble(value), buffer, sizeof(buffer));
            label_set_add(labels, true, keystring, buffer);
        } else if (PyNumber_Check(value)) {
            PyObject *tmp = PyNumber_Long(value);
            long long int lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                char buffer[ITOA_MAX];
                itoa(lli, buffer);
                label_set_add(labels, true, keystring, buffer);
            } else {
                PyErr_Clear();
                long long unsigned llu = PyLong_AsUnsignedLongLong(tmp);
                if (!PyErr_Occurred() && (llu == (uint64_t)llu)) {
                    char buffer[ITOA_MAX];
                    uitoa(llu, buffer);
                    label_set_add(labels, true, keystring, buffer);
                }
            }
            Py_XDECREF(tmp);
        } else {
            const char *string = cpy_unicode_or_bytes_to_string(&value);
            if (string != NULL) {
                label_set_add(labels, true, keystring, string);
            } else {
                PyErr_Clear();
                PyObject *tmp = PyObject_Str(value);
                string = cpy_unicode_or_bytes_to_string(&tmp);
                if (string != NULL)
                    label_set_add(labels, true, keystring, string);
                Py_XDECREF(tmp);
            }
        }

        if (PyErr_Occurred())
            cpy_log_exception("building labels");

        Py_XDECREF(value);
        Py_DECREF(key);
    }
    Py_XDECREF(l);
    return 0;
}

PyObject *cpy_metric_repr(PyObject *s)
{
    PyObject *tmp;
    static PyObject *l_open, *l_time, *l_interval, *l_labels;
    Metric *self = (Metric *)s;

    if (l_open == NULL)
        l_open = cpy_string_to_unicode_or_bytes("(");
    if (l_time == NULL)
        l_time = cpy_string_to_unicode_or_bytes(",time=");
    if (l_interval == NULL)
        l_interval = cpy_string_to_unicode_or_bytes(",interval=");
    if (l_labels == NULL)
        l_labels = cpy_string_to_unicode_or_bytes(",labels=");

    if ((l_open == NULL) || (l_time == NULL) || (l_interval == NULL) || (l_labels == NULL))
        return NULL;

    PyObject *ret = cpy_string_to_unicode_or_bytes(s->ob_type->tp_name);

    CPY_STRCAT(&ret, l_open);

    if (self->time != 0) {
        CPY_STRCAT(&ret, l_time);
        tmp = PyFloat_FromDouble(self->time);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->interval != 0) {
        CPY_STRCAT(&ret, l_interval );
        tmp = PyFloat_FromDouble(self->interval);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->labels && (!PyDict_Check(self->labels) || PyDict_Size(self->labels) > 0)) {
        CPY_STRCAT(&ret, l_labels);
        tmp = PyObject_Repr(self->labels);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    return ret;
}
