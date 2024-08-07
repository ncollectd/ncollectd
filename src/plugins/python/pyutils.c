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

    if (PyDict_Size(dict) <= 0)
        return 0;

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value)) {
        Py_INCREF(key);
        const char *keystring = cpy_unicode_or_bytes_to_string(&key);
        if (keystring == NULL) {
            Py_XDECREF(key);
            PyErr_Clear();
            continue;
        }

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
            Py_INCREF(value);
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
            Py_XDECREF(value);
        }

        Py_XDECREF(key);

        if (PyErr_Occurred())
            cpy_log_exception("building labels");
    }

    return 0;
}

int cpy_build_state_set(PyObject *dict, state_set_t *set)
{
    if ((dict == NULL) || (dict == Py_None))
        return -1;

    if (PyDict_Size(dict) <= 0)
        return 0;

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value)) {
        Py_INCREF(key);
        const char *keystring = cpy_unicode_or_bytes_to_string(&key);
        if (keystring == NULL) {
            Py_XDECREF(key);
            PyErr_Clear();
            continue;
        }

        if (value == Py_True) {
            state_set_add(set, keystring, true);
        } else if (value == Py_False) {
            state_set_add(set, keystring, false);
        } else if (PyFloat_Check(value)) {
            state_set_add(set, keystring, PyFloat_AsDouble(value) == 0.0 ? false : true);
        } else if (PyNumber_Check(value)) {
            PyObject *tmp = PyNumber_Long(value);
            long long int lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                state_set_add(set, keystring, lli == 0 ? false : true);
            } else {
                PyErr_Clear();
                long long unsigned llu = PyLong_AsUnsignedLongLong(tmp);
                if (!PyErr_Occurred() && (llu == (uint64_t)llu)) {
                    state_set_add(set, keystring, llu == 0 ? false : true);
                }
            }
            Py_XDECREF(tmp);
        }

        Py_XDECREF(key);

        if (PyErr_Occurred())
            cpy_log_exception("building state_set");
    }

    return 0;
}

histogram_t *cpy_build_histogram(PyObject *list)
{
    if ((list == NULL) || (list == Py_None))
        return NULL;

    ssize_t size = PySequence_Length(list);
    if (size <= 0)
        return NULL;

    histogram_t *h = calloc(1, sizeof(histogram_t)+sizeof(histogram_bucket_t)*size);
    if (h == NULL)
        return NULL;

    h->sum = 0;
    h->num = size;

    for (ssize_t i = 0; i < size; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(list, (int)i); /* Borrowed reference. */
        if (item == NULL) {
            free(h);
            return NULL;
        }

        if (!PyTuple_Check(item) && !PyList_Check(item)) {
            free(h);
            return NULL;
        }

        if (PySequence_Length(item) != 2) {
            free(h);
            return NULL;
        }

        PyObject *counter = PySequence_Fast_GET_ITEM(item, 0);  /* Borrowed reference. */
        PyObject *maximum =  PySequence_Fast_GET_ITEM(item, 1);  /* Borrowed reference. */

        if ((counter == NULL) || (maximum == NULL)) {
            free(h);
            return NULL;
        }

        h->buckets[i].counter = 0;
        if (PyFloat_Check(counter)) {
            h->buckets[i].counter = PyFloat_AsDouble(counter);
        } else if (PyNumber_Check(counter)) {
            PyObject *tmp = PyNumber_Long(counter);
            long long int lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                h->buckets[i].counter = lli;
            } else {
                PyErr_Clear();
                long long unsigned llu = PyLong_AsUnsignedLongLong(tmp);

                if (!PyErr_Occurred() && (llu == (uint64_t)llu)) {
                    h->buckets[i].counter = llu;
                }
            }
            Py_XDECREF(tmp);
        }

        h->buckets[i].maximum = 0;
        if (PyFloat_Check(maximum)) {
            h->buckets[i].maximum = PyFloat_AsDouble(maximum);
        } else if (PyNumber_Check(counter)) {
            PyObject *tmp = PyNumber_Long(maximum);
            long long int lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                h->buckets[i].maximum = lli;
            } else {
                PyErr_Clear();
                long long unsigned llu = PyLong_AsUnsignedLongLong(tmp);

                if (!PyErr_Occurred() && (llu == (uint64_t)llu)) {
                    h->buckets[i].maximum = llu;
                }
            }
            Py_XDECREF(tmp);
        }

        if (PyErr_Occurred())
            cpy_log_exception("building state_set");
    }

    return 0;
}

summary_t *cpy_build_summary(PyObject *list)
{
    if ((list == NULL) || (list == Py_None))
        return NULL;

    ssize_t size = PySequence_Length(list);
    if (size <= 0)
        return NULL;

    summary_t *s = calloc(1, sizeof(summary_t)+sizeof(summary_quantile_t)*size);
    if (s == NULL)
        return NULL;

    s->sum = 0;
    s->count  = 0;
    s->num = size;

    for (ssize_t i = 0; i < size; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(list, (int)i); /* Borrowed reference. */
        if (item == NULL) {
            free(s);
            return NULL;
        }

        if (!PyTuple_Check(item) && !PyList_Check(item)) {
            free(s);
            return NULL;
        }

        if (PySequence_Length(item) != 2) {
            free(s);
            return NULL;
        }

        PyObject *quantile = PySequence_Fast_GET_ITEM(item, 0);  /* Borrowed reference. */
        PyObject *value =  PySequence_Fast_GET_ITEM(item, 1);  /* Borrowed reference. */

        if ((quantile == NULL) || (value == NULL)) {
            free(s);
            return NULL;
        }

        s->quantiles[i].quantile = 0;
        if (PyFloat_Check(quantile)) {
            s->quantiles[i].quantile = PyFloat_AsDouble(quantile);
        } else if (PyNumber_Check(quantile)) {
            PyObject *tmp = PyNumber_Long(quantile);
            long long int lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                s->quantiles[i].quantile = lli;
            } else {
                PyErr_Clear();
                long long unsigned llu = PyLong_AsUnsignedLongLong(tmp);

                if (!PyErr_Occurred() && (llu == (uint64_t)llu)) {
                    s->quantiles[i].quantile = llu;
                }
            }
            Py_XDECREF(tmp);
        }

        s->quantiles[i].value = 0;
        if (PyFloat_Check(value)) {
            s->quantiles[i].value = PyFloat_AsDouble(value);
        } else if (PyNumber_Check(quantile)) {
            PyObject *tmp = PyNumber_Long(value);
            long long int lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                s->quantiles[i].value = lli;
            } else {
                PyErr_Clear();
                long long unsigned llu = PyLong_AsUnsignedLongLong(tmp);

                if (!PyErr_Occurred() && (llu == (uint64_t)llu)) {
                    s->quantiles[i].value = llu;
                }
            }
            Py_XDECREF(tmp);
        }

        if (PyErr_Occurred())
            cpy_log_exception("building state_set");
    }

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
        l_time = cpy_string_to_unicode_or_bytes("time=");
    if (l_interval == NULL)
        l_interval = cpy_string_to_unicode_or_bytes(",interval=");
    if (l_labels == NULL)
        l_labels = cpy_string_to_unicode_or_bytes(",labels=");

    if ((l_open == NULL) || (l_time == NULL) || (l_interval == NULL) || (l_labels == NULL))
        return NULL;

    PyObject *ret = cpy_string_to_unicode_or_bytes(s->ob_type->tp_name);

    CPY_STRCAT(&ret, l_open);

    CPY_STRCAT(&ret, l_time);
    tmp = PyFloat_FromDouble(self->time);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

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
