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

#define POW_2_64 18446744073709551616.0

typedef struct {
    bool is_unsigned;
    int64_t int64;
    uint64_t uint64;
} cpy_integer_t;


static int cpy_label_set_add(label_set_t *labels, const char *name, const char *value)
{
    int status = label_set_add(labels, true, name, value);
    if (status == 0)
        return 0;

    if (status == EINVAL)
        PyErr_Format(PyExc_ValueError, "invalid label name: '%s'", name);
    else
        PyErr_NoMemory();
    return -1;
}

static int cpy_state_set_add(state_set_t *set, const char *name, bool enabled)
{
    int status = state_set_add(set, name, enabled);
    if (status == 0)
        return 0;

    if (status == EINVAL)
        PyErr_Format(PyExc_ValueError, "invalid state name: '%s'", name);
    else
        PyErr_NoMemory();
    return -1;
}

static int cpy_number_as_integer(PyObject *value, cpy_integer_t *integer)
{
    PyObject *tmp = PyNumber_Long(value); /* New reference. */
    if (tmp == NULL)
        return -1;

    int overflow = 0;
    long long lli = PyLong_AsLongLongAndOverflow(tmp, &overflow);
    if ((lli == -1) && PyErr_Occurred()) {
        Py_DECREF(tmp);
        return -1;
    }

    if (overflow == 0) {
        Py_DECREF(tmp);
        integer->int64 = lli;
        integer->is_unsigned = false;
        return 0;
    }

    unsigned long long llu = PyLong_AsUnsignedLongLong(tmp);
    Py_DECREF(tmp);
    if ((llu == (unsigned long long)-1) && PyErr_Occurred())
        return -1;

    integer->uint64 = llu;
    integer->is_unsigned = true;
    return 0;
}

static int cpy_number_as_uint64(PyObject *value, uint64_t *ret)
{
    if (PyFloat_Check(value)) {
        double d = PyFloat_AsDouble(value);
        /* Also false for NaN. */
        if (!((d >= 0.0) && (d < POW_2_64))) {
            PyErr_SetString(PyExc_ValueError, "counter must be a number between 0 and 2^64");
            return -1;
        }
        *ret = (uint64_t)d;
        return 0;
    }

    cpy_integer_t integer = {0};
    if (cpy_number_as_integer(value, &integer) != 0)
        return -1;

    if (!integer.is_unsigned && (integer.int64 < 0)) {
        PyErr_SetString(PyExc_ValueError, "counter must be a number between 0 and 2^64");
        return -1;
    }

    *ret = integer.is_unsigned ? integer.uint64 : (uint64_t)integer.int64;
    return 0;
}

static int cpy_number_as_double(PyObject *value, double *ret)
{
    double d = PyFloat_AsDouble(value);
    if ((d == -1.0) && PyErr_Occurred())
        return -1;

    *ret = d;
    return 0;
}


int cpy_build_labels(PyObject *dict, label_set_t *labels)
{
    if ((dict == NULL) || (dict == Py_None))
        return 0;

    if (!PyDict_Check(dict)) {
        PyErr_Format(PyExc_TypeError, "expected a dict of labels, not %.200s",
                     Py_TYPE(dict)->tp_name);
        return -1;
    }

    if (PyDict_Size(dict) == 0)
        return 0;

    PyObject *entries = PyDict_Items(dict);
    if (entries == NULL)
        return -1;

    Py_ssize_t size = PyList_GET_SIZE(entries);
    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *pair = PyList_GET_ITEM(entries, i);
        PyObject *key = PyTuple_GET_ITEM(pair, 0);
        PyObject *value = PyTuple_GET_ITEM(pair, 1);

        if (!IS_BYTES_OR_UNICODE(key)) {
            PyErr_Format(PyExc_TypeError, "label names must be str, not %.200s",
                         Py_TYPE(key)->tp_name);
            goto error;
        }
        Py_INCREF(key);
        const char *keystring = cpy_unicode_or_bytes_to_string(&key);
        if (keystring == NULL) {
            Py_DECREF(key);
            goto error;
        }

        int status = 0;
        if (value == Py_True) {
            status = cpy_label_set_add(labels, keystring, "true");
        } else if (value == Py_False) {
            status = cpy_label_set_add(labels, keystring, "false");
        } else if (PyFloat_Check(value)) {
            char buffer[DTOA_MAX];
            dtoa(PyFloat_AsDouble(value), buffer, sizeof(buffer));
            status = cpy_label_set_add(labels, keystring, buffer);
        } else if (PyNumber_Check(value)) {
            cpy_integer_t integer = {0};
            status = cpy_number_as_integer(value, &integer);
            if (status == 0) {
                char buffer[ITOA_MAX];
                if (integer.is_unsigned)
                    uitoa(integer.uint64, buffer);
                else
                    itoa(integer.int64, buffer);
                status = cpy_label_set_add(labels, keystring, buffer);
            }
        } else {
            Py_INCREF(value);
            const char *string = cpy_unicode_or_bytes_to_string(&value);
            if (string != NULL) {
                status = cpy_label_set_add(labels, keystring, string);
            } else {
                PyErr_Clear();
                PyObject *tmp = PyObject_Str(value);
                if (tmp != NULL) {
                    string = cpy_unicode_or_bytes_to_string(&tmp);
                    if (string != NULL)
                        status = cpy_label_set_add(labels, keystring, string);
                    Py_XDECREF(tmp);
                }
            }
            Py_XDECREF(value);
        }

        Py_XDECREF(key);

        if ((status != 0) || PyErr_Occurred())
            goto error;
    }
    
    Py_DECREF(entries);
    return 0;

error:
    Py_DECREF(entries);
    return -1;
}

int cpy_build_state_set(PyObject *dict, state_set_t *set)
{
    if ((dict == NULL) || (dict == Py_None))
        return 0;

    if (!PyDict_Check(dict)) {
        PyErr_Format(PyExc_TypeError, "expected a dict of labels, not %.200s",
                     Py_TYPE(dict)->tp_name);
        return -1;
    }

    if (PyDict_Size(dict) == 0)
        return 0;

    PyObject *entries = PyDict_Items(dict);
    if (entries == NULL)
        return -1;

    Py_ssize_t size = PyList_GET_SIZE(entries);
    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *pair = PyList_GET_ITEM(entries, i);
        PyObject *key = PyTuple_GET_ITEM(pair, 0);
        PyObject *value = PyTuple_GET_ITEM(pair, 1);

        if (!IS_BYTES_OR_UNICODE(key)) {
            PyErr_Format(PyExc_TypeError, "state names must be str, not %.200s",
                         Py_TYPE(key)->tp_name);
            goto error;
        }

        Py_INCREF(key);
        const char *keystring = cpy_unicode_or_bytes_to_string(&key);
        if (keystring == NULL) {
            Py_DECREF(key);
            goto error;
        }

        int status = 0;
        if (value == Py_True) {
            status = cpy_state_set_add(set, keystring, true);
        } else if (value == Py_False) {
            status = cpy_state_set_add(set, keystring, false);
        } else if (PyFloat_Check(value)) {
            status = cpy_state_set_add(set, keystring,
                                       PyFloat_AsDouble(value) == 0.0 ? false : true);
        } else if (PyNumber_Check(value)) {
            cpy_integer_t integer = {0};
            status = cpy_number_as_integer(value, &integer);
            if (status == 0)
                status = cpy_state_set_add(set, keystring,
                                           integer.is_unsigned ? (integer.uint64 != 0)
                                                               : (integer.int64 != 0));
        } else {
            PyErr_Format(PyExc_TypeError, "state values must be bool, not %.200s",
                         Py_TYPE(value)->tp_name);
            status = -1;
        }

        Py_DECREF(key);

        if ((status != 0) || PyErr_Occurred())
            goto error;
    }

    Py_DECREF(entries);
    return 0;

error:
    Py_DECREF(entries);
    return -1;
}

static PyObject *cpy_pair_snapshot(PyObject *sequence)
{
    Py_INCREF(sequence);
    PyObject *outer = PySequence_Tuple(sequence);
    Py_DECREF(sequence);
    if (outer == NULL)
        return NULL;

    Py_ssize_t size = PyTuple_GET_SIZE(outer);
    PyObject *pairs = PyTuple_New(size);
    if (pairs == NULL) {
        Py_DECREF(outer);
        return NULL;
    }

    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *item = PyTuple_GET_ITEM(outer, i);
        if (!PyTuple_Check(item) && !PyList_Check(item)) {
            PyErr_SetString(PyExc_TypeError, "expected a list or tuple pair");
            Py_DECREF(pairs);
            Py_DECREF(outer);
            return NULL;
        }

        PyObject *pair = PySequence_Tuple(item);
        if (pair == NULL) {
            Py_DECREF(pairs);
            Py_DECREF(outer);
            return NULL;
        }

        PyTuple_SET_ITEM(pairs, i, pair);
    }

    Py_DECREF(outer);

    return pairs;
}

histogram_t *cpy_build_histogram(PyObject *list)
{
    if ((list == NULL) || (list == Py_None))
        return NULL;

    PyObject *seq_list = cpy_pair_snapshot(list);
    if (seq_list == NULL)
         return NULL;
    
    Py_ssize_t size = PySequence_Fast_GET_SIZE(seq_list); 
    if (size < 0) {
        Py_DECREF(seq_list);
        return NULL;
    }

    histogram_t *h = calloc(1, sizeof(histogram_t)+sizeof(histogram_bucket_t)*size);
    if (h == NULL) {
        Py_DECREF(seq_list);
        return NULL;
    }

    h->sum = 0;
    h->num = size;

    PyObject **items = PySequence_Fast_ITEMS(seq_list);

    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *item = items[i]; /* Borrowed reference. */
        if (item == NULL) {
            free(h);
            Py_DECREF(seq_list);
            return NULL;
        }

        if (!PyTuple_Check(item) && !PyList_Check(item)) {
            free(h);
            Py_DECREF(seq_list);
            return NULL;
        }

        if (PySequence_Length(item) != 2) {
            free(h);
            Py_DECREF(seq_list);
            return NULL;
        }

        PyObject *counter = PySequence_Fast_GET_ITEM(item, 0);  /* Borrowed reference. */
        PyObject *maximum =  PySequence_Fast_GET_ITEM(item, 1);  /* Borrowed reference. */

        if ((counter == NULL) || (maximum == NULL)) {
            free(h);
            Py_DECREF(seq_list);
            return NULL;
        }

        if ((cpy_number_as_uint64(counter, &h->buckets[i].counter) != 0) ||
            (cpy_number_as_double(maximum, &h->buckets[i].maximum) != 0)) {
            free(h);
            Py_DECREF(seq_list);
            return NULL;
        }
    }

    Py_DECREF(seq_list);

    return h;
}

summary_t *cpy_build_summary(PyObject *list)
{
    if ((list == NULL) || (list == Py_None))
        return NULL;

    PyObject *seq_list = cpy_pair_snapshot(list);
    if (seq_list == NULL)
         return NULL;
    
    Py_ssize_t size = PySequence_Fast_GET_SIZE(seq_list); 
    if (size < 0) {
        Py_DECREF(seq_list);
        return NULL;
    }

    summary_t *s = calloc(1, sizeof(summary_t)+sizeof(summary_quantile_t)*size);
    if (s == NULL) {
        Py_DECREF(seq_list);
        return NULL;
    }

    s->sum = 0;
    s->count  = 0;
    s->num = size;

    PyObject **items = PySequence_Fast_ITEMS(seq_list);

    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject *item = items[i]; /* Borrowed reference. */
        if (item == NULL) {
            free(s);
            Py_DECREF(seq_list);
            return NULL;
        }

        if (!PyTuple_Check(item) && !PyList_Check(item)) {
            free(s);
            Py_DECREF(seq_list);
            return NULL;
        }

        if (PySequence_Length(item) != 2) {
            free(s);
            Py_DECREF(seq_list);
            return NULL;
        }

        PyObject *quantile = PySequence_Fast_GET_ITEM(item, 0);  /* Borrowed reference. */
        PyObject *value =  PySequence_Fast_GET_ITEM(item, 1);  /* Borrowed reference. */

        if ((quantile == NULL) || (value == NULL)) {
            free(s);
            Py_DECREF(seq_list);
            return NULL;
        }

        if ((cpy_number_as_double(quantile, &s->quantiles[i].quantile) != 0) ||
            (cpy_number_as_double(value, &s->quantiles[i].value) != 0)) {
            free(s);
            Py_DECREF(seq_list);
            return NULL;
        }
    }

    Py_DECREF(seq_list);

    return s;
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
    if (ret == NULL)
        return NULL;

    CPY_STRCAT(&ret, l_time);
    tmp = PyFloat_FromDouble(self->time);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);
    if (ret == NULL)
        return NULL;

    if (self->interval != 0) {
        CPY_STRCAT(&ret, l_interval );
        if (ret == NULL)
            return NULL;
        tmp = PyFloat_FromDouble(self->interval);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
        if (ret == NULL)
            return NULL;
    }

    if (self->labels && (!PyDict_Check(self->labels) || PyDict_Size(self->labels) > 0)) {
        CPY_STRCAT(&ret, l_labels);
        if (ret == NULL)
            return NULL;
        tmp = PyObject_Repr(self->labels);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    return ret;
}
