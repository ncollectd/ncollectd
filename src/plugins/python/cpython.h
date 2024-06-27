/* SPDX-License-Identifier: GPL-2.0-only OR MIT                     */
/* SPDX-FileCopyrightText: Copyright (C) 2009 Sven Trenkel          */
/* SPDX-FileContributor: Sven Trenkel <collectd at semidefinite.de> */

#pragma once

#include <Python.h>
/* Some python versions don't include this by default. */
#if PY_VERSION_HEX < 0x030B0000
/* Python 3.11 move longintrepr.h to cpython/longintrepr.h and it's always included */
#include <longintrepr.h>
#endif

/* These two macros are basically Py_BEGIN_ALLOW_THREADS and
 * Py_BEGIN_ALLOW_THREADS
 * from the other direction. If a Python thread calls a C function
 * Py_BEGIN_ALLOW_THREADS is used to allow other python threads to run because
 * we don't intend to call any Python functions.
 *
 * These two macros are used whenever a C thread intends to call some Python
 * function, usually because some registered callback was triggered.
 * Just like Py_BEGIN_ALLOW_THREADS it opens a block so these macros have to be
 * used in pairs. They acquire the GIL, create a new Python thread state and
 * swap
 * the current thread state with the new one. This means this thread is now
 * allowed
 * to execute Python code. */

#define CPY_LOCK_THREADS                                                       \
    {                                                                          \
        PyGILState_STATE gil_state;                                            \
        gil_state = PyGILState_Ensure();

#define CPY_RETURN_FROM_THREADS                                                \
        PyGILState_Release(gil_state);                                         \
        return

#define CPY_RELEASE_THREADS                                                    \
        PyGILState_Release(gil_state);                                         \
  }

/* This macro is a shortcut for calls like
 * x = PyObject_Repr(x);
 * This can't be done like this example because this would leak
 * a reference the the original x and crash in case of x == NULL.
 * This calling syntax is less than elegant but it works, saves
 * a lot of lines and avoids potential refcount errors. */

#define CPY_SUBSTITUTE(func, a, ...)                                           \
    do {                                                                       \
        if ((a) != NULL) {                                                     \
            PyObject *__tmp = (a);                                             \
            (a) = func(__VA_ARGS__);                                           \
            Py_DECREF(__tmp);                                                  \
        }                                                                      \
    } while (0)

/* Python3 compatibility layer. To keep the actual code as clean as possible
 * do a lot of defines here. */

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

#ifdef IS_PY3K

#define PyInt_FromLong PyLong_FromLong
#define CPY_INIT_TYPE PyVarObject_HEAD_INIT(NULL, 0)
#define IS_BYTES_OR_UNICODE(o) (PyUnicode_Check(o) || PyBytes_Check(o))
#define CPY_STRCAT_AND_DEL(a, b)                                               \
    do {                                                                       \
        CPY_STRCAT((a), (b));                                                  \
        Py_XDECREF((b));                                                       \
    } while (0)

static inline void CPY_STRCAT(PyObject **a, PyObject *b)
{
    PyObject *ret;

    if (!a || !*a)
        return;

    ret = PyUnicode_Concat(*a, b);
    Py_DECREF(*a);
    *a = ret;
}

#else

#define CPY_INIT_TYPE PyObject_HEAD_INIT(NULL) 0,
#define IS_BYTES_OR_UNICODE(o) (PyUnicode_Check(o) || PyString_Check(o))
#define CPY_STRCAT_AND_DEL PyString_ConcatAndDel
#define CPY_STRCAT PyString_Concat

#endif

static inline const char *cpy_unicode_or_bytes_to_string(PyObject **o)
{
    if (PyUnicode_Check(*o)) {
        PyObject *tmp;
        tmp = PyUnicode_AsEncodedString(*o, NULL, NULL); /* New reference. */
        if (tmp == NULL)
            return NULL;
        Py_DECREF(*o);
        *o = tmp;
    }
#ifdef IS_PY3K
    return PyBytes_AsString(*o);
#else
    return PyString_AsString(*o);
#endif
}

static inline PyObject *cpy_string_to_unicode_or_bytes(const char *buf)
{
#ifdef IS_PY3K
    /* Python3 preferrs unicode */
    PyObject *ret = PyUnicode_Decode(buf, strlen(buf), NULL, NULL);
    if (ret != NULL)
        return ret;
      PyErr_Clear();
    return PyBytes_FromString(buf);
#else
    return PyString_FromString(buf);
#endif
}

void cpy_log_exception(const char *context);

/* Python object declarations. */

typedef struct {
    PyObject_HEAD       /* No semicolon! */
    PyObject *parent;   /* Config */
    PyObject *key;      /* String */
    PyObject *values;   /* Sequence */
    PyObject *children; /* Sequence */
} Config;
extern PyTypeObject ConfigType;

typedef struct {
    PyObject_HEAD          /* No semicolon! */
    double time;
    double interval;
    PyObject *labels;      /* dict */
} Metric;
extern PyTypeObject MetricType;
#define Metric_New()                                                           \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricType, (void *)0)

typedef struct {
    Metric metric;
    double value;
} MetricUnknownDouble;
extern PyTypeObject MetricUnknownDoubleType;
#define MetricUnknownDouble_New(v)                                              \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricUnknownDoubleType, (v), (void *)0)

typedef struct {
    Metric metric;
    int64_t value;
} MetricUnknownLong;
extern PyTypeObject MetricUnknownLongType;
#define MetricUnknownLong_New(v)                                                \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricUnknownLongType, (v), (void *)0)

typedef struct {
    Metric metric;
    double value;
} MetricGaugeDouble;
extern PyTypeObject MetricGaugeDoubleType;
#define MetricGaugeDouble_New(v)                                                \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricGaugeDoubleType, (v), (void *)0)

typedef struct {
    Metric metric;
    int64_t value;
} MetricGaugeLong;
extern PyTypeObject MetricGaugeLongType;
#define MetricGaugeLong_New(v)                                                  \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricGaugeLongType, (v), (void *)0)

typedef struct {
    Metric metric;
    uint64_t value;
} MetricCounterULong;
extern PyTypeObject MetricCounterULongType;
#define MetricCounterULong_New(v)                                               \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricCounterULongType, (v), (void *)0)

typedef struct {
    Metric metric;
    double value;
} MetricCounterDouble;
extern PyTypeObject MetricCounterDoubleType;
#define MetricCounterDouble_New(v)                                              \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricCounterDoubleType, (v), (void *)0)

typedef struct {
    Metric metric;
    PyObject *set; /* dict */
} MetricStateSet;
extern PyTypeObject MetricStateSetType;
#define MetricStateSet_New(s)                                                   \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricStateSetType, (s), (void *)0)

typedef struct {
    Metric metric;
    PyObject *info;
} MetricInfo;
extern PyTypeObject MetricInfoType;
#define MetricInfo_New(l)                                                       \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricInfoType, (l), (void *)0)

typedef struct {
    Metric metric;
    double sum;
    uint64_t count;
    PyObject *quantiles; /* Sequence */
} MetricSummary;
extern PyTypeObject MetricSummaryType;
#define MetricSummary_New(s, c, q)                                              \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricSummaryType, (s), (c), (q), (void *)0)

typedef struct {
    Metric metric;
    double sum;
    PyObject *buckets;  /* Sequence */
} MetricHistogram;
extern PyTypeObject MetricHistogramType;
#define MetricHistogram_New(s, b)                                               \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricHistogramType, (s), (b), (void *)0)

typedef struct {
    Metric metric;
    double sum;
    PyObject *buckets;
} MetricGaugeHistogram;
extern PyTypeObject MetricGaugeHistogramType;
#define MetricGaugeHistogram_New(s, b)                                          \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricGaugeHistogramType, (s), (b), (void *)0)

typedef struct {
    PyObject_HEAD      /* No semicolon! */
    PyObject *name;    /* String */
    PyObject *help;    /* String */
    PyObject *unit;    /* String */
    int type;
    PyObject *metrics; /* Sequence */
} MetricFamily;
extern PyTypeObject MetricFamilyType;
#define MetricFamily_New(t, n)                                                     \
    PyObject_CallFunctionObjArgs((PyObject *)&MetricFamilyType, (t), (n), (void *)0)

typedef struct {
    PyObject_HEAD          /* No semicolon! */
    PyObject *name;        /* String */
    int severity;
    double time;
    PyObject *labels;      /* dict */
    PyObject *annotations; /* dict */
} Notification;
extern PyTypeObject NotificationType;
#define Notification_New(n)                                                     \
    PyObject_CallFunctionObjArgs((PyObject *)&NotificationType, (n), (void *)0)

int cpy_build_labels(PyObject *dict, label_set_t *labels);
int cpy_build_state_set(PyObject *dict, state_set_t *set);
histogram_t *cpy_build_histogram(PyObject *list);
summary_t *cpy_build_summary(PyObject *list);
PyObject *cpy_metric_repr(PyObject *s);

