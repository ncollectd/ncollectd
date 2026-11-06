// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009  Sven Trenkel
// SPDX-FileContributor: Sven Trenkel <collectd at semidefinite.de>

#include <Python.h>
#include <structmember.h>

#include <signal.h>

#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wpedantic"

#include "plugin.h"
#include "libutils/common.h"

#include "cpython.h"

#define PY_VERSION_ATLEAST(major, minor)                            \
  (PY_MAJOR_VERSION > major) ||                                     \
      ((PY_MAJOR_VERSION == major) && (PY_MINOR_VERSION >= minor))

typedef struct cpy_callback_s {
    char *name;
    PyObject *callback;
    PyObject *data;
    struct cpy_callback_s *next;
} cpy_callback_t;

static char log_doc[] = "This function sends a string to all logging plugins.";

static char unregister_doc[] =
    "Unregisters a callback. This function needs exactly one parameter either\n"
    "the function to unregister or the callback identifier to unregister.";

static char reg_log_doc[] =
    "register_log(callback[, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function for log messages.\n"
    "\n"
    "'callback' is a callable object that will be called every time something\n"
    "        is logged.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function every time it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called with two or three parameters:\n"
    "severity: An integer that should be compared to the LOG_ constants.\n"
    "message: The text to be logged.\n"
    "data: The optional data parameter passed to the register function.\n"
    "        If the parameter was omitted it will be omitted here, too.";

static char reg_init_doc[] =
    "register_init(callback[, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function that will be executed once after the "
    "config.\n"
    "file has been read, all plugins heve been loaded and the ncollectd has\n"
    "forked into the background.\n"
    "\n"
    "'callback' is a callable object that will be executed.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function when it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called without parameters, except for\n"
    "data if it was supplied.";

static char reg_config_doc[] =
    "register_config(callback[, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function for config file entries.\n"
    "'callback' is a callable object that will be called for every config "
    "block.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function every time it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called with one or two parameters:\n"
    "config: A Config object.\n"
    "data: The optional data parameter passed to the register function.\n"
    "        If the parameter was omitted it will be omitted here, too.";

static char reg_read_doc[] =
    "register_read(callback[, interval][, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function for reading data. It will just be called\n"
    "in a fixed interval to signal that it's time to dispatch new values.\n"
    "'callback' is a callable object that will be called every time something\n"
    "        is logged.\n"
    "'interval' is the number of seconds between between calls to the "
    "callback\n"
    "        function. Full float precision is supported here.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function every time it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called without parameters, except for\n"
    "data if it was supplied.";

static char reg_write_doc[] =
    "register_write(callback[, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function to receive values dispatched by other "
    "plugins.\n"
    "'callback' is a callable object that will be called every time a value\n"
    "        is dispatched.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function every time it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called with one or two parameters:\n"
    "values: A Values object which is a copy of the dispatched values.\n"
    "data: The optional data parameter passed to the register function.\n"
    "        If the parameter was omitted it will be omitted here, too.";

static char reg_notification_doc[] =
    "register_notification(callback[, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function for notifications.\n"
    "'callback' is a callable object that will be called every time a "
    "notification\n"
    "        is dispatched.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function every time it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called with one or two parameters:\n"
    "notification: A copy of the notification that was dispatched.\n"
    "data: The optional data parameter passed to the register function.\n"
    "        If the parameter was omitted it will be omitted here, too.";

static char reg_shutdown_doc[] =
    "register_shutdown(callback[, data][, name]) -> identifier\n"
    "\n"
    "Register a callback function for ncollectd shutdown.\n"
    "'callback' is a callable object that will be called once ncollectd is\n"
    "        shutting down.\n"
    "'data' is an optional object that will be passed back to the callback\n"
    "        function if it is called.\n"
    "'name' is an optional identifier for this callback. The default name\n"
    "        is 'python.<module>'.\n"
    "        Every callback needs a unique identifier, so if you want to\n"
    "        register this callback multiple time from the same module you need\n"
    "        to specify a name here.\n"
    "'identifier' is the full identifier assigned to this callback.\n"
    "\n"
    "The callback function will be called with no parameters except for\n"
    "        data if it was supplied.";

static char NCollectdError_doc[] =
    "Basic exception for ncollectd Python scripts.\n"
    "\n"
    "Throwing this exception will not cause a stacktrace to be logged, \n"
    "even if LogTraces is enabled in the config.";

static pthread_t main_thread;
static PyOS_sighandler_t python_sigint_handler;
static bool do_interactive;

/* This is our global thread state. Python saves some stuff in thread-local
 * storage. So if we allow the interpreter to run in the background
 * (the scriptwriters might have created some threads from python), we have
 * to save the state so we can resume it later after shutdown. */

static PyThreadState *state;

static PyObject *sys_path, *cpy_format_exception, *NCollectdError;

static cpy_callback_t *cpy_config_callbacks;
static cpy_callback_t *cpy_init_callbacks;
static cpy_callback_t *cpy_shutdown_callbacks;

/* Make sure to hold the GIL while modifying these. */
static int cpy_shutdown_triggered;
static int cpy_num_callbacks;

static pid_t config_pid;

static void cpy_destroy_user_data(void *data)
{
    cpy_callback_t *c = data;
    free(c->name);
    CPY_LOCK_THREADS
    Py_DECREF(c->callback);
    Py_XDECREF(c->data);
    free(c);
    --cpy_num_callbacks;
    if (!cpy_num_callbacks && cpy_shutdown_triggered) {
        Py_Finalize();
        return;
    }
    CPY_RELEASE_THREADS
}

/* You must hold the GIL to call this function!
 * But if you managed to extract the callback parameter then you probably
 * already do. */

static void cpy_build_name(char *buf, size_t size, PyObject *callback, const char *name)
{
    const char *module = NULL;
    PyObject *mod = NULL;

    if (name != NULL) {
        ssnprintf(buf, size, "%s", name);
        return;
    }

    mod = PyObject_GetAttrString(callback, "__module__"); /* New reference. */
    if (mod != NULL)
        module = cpy_unicode_or_bytes_to_string(&mod);

    if (module != NULL) {
        ssnprintf(buf, size, "%s", module);
        Py_XDECREF(mod);
        PyErr_Clear();
        return;
    }
    Py_XDECREF(mod);

    ssnprintf(buf, size, "%p", (void *)callback);
    PyErr_Clear();
}

void cpy_log_exception(const char *context)
{
    int l = 0, ncollectd_error;
    const char *typename = NULL, *message = NULL;
    PyObject *type, *value, *traceback, *tn, *m, *list;

    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    if (type == NULL)
        return;
    ncollectd_error = PyErr_GivenExceptionMatches(value, NCollectdError);
    tn = PyObject_GetAttrString(type, "__name__"); /* New reference. */
    m = PyObject_Str(value); /* New reference. */
    if (tn != NULL)
        typename = cpy_unicode_or_bytes_to_string(&tn);
    if (m != NULL)
        message = cpy_unicode_or_bytes_to_string(&m);
    if (typename == NULL)
        typename = "NamelessException";
    if (message == NULL)
        message = "N/A";

    PyErr_Clear();

    Py_BEGIN_ALLOW_THREADS;
    if (ncollectd_error) {
        PLUGIN_WARNING("%s in %s: %s", typename, context, message);
    } else {
        PLUGIN_ERROR("Unhandled python exception in %s: %s: %s", context, typename, message);
    }
    Py_END_ALLOW_THREADS;

    Py_XDECREF(tn);
    Py_XDECREF(m);

    if (!cpy_format_exception || !traceback || ncollectd_error) {
        PyErr_Clear();
        Py_DECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);
        return;
    }
    /* New reference. Steals references from "type", "value" and "traceback". */
    list = PyObject_CallFunction(cpy_format_exception, "NNN", type, value, traceback);
    if (list)
        l = PyObject_Length(list);

    for (int i = 0; i < l; ++i) {
        PyObject *line;
        char const *msg;
        char *cpy;

        line = PyList_GET_ITEM(list, i); /* Borrowed reference. */
        Py_INCREF(line);

        msg = cpy_unicode_or_bytes_to_string(&line);
        if (msg == NULL) {
            Py_DECREF(line);
            continue;
        }

        cpy = strdup(msg);
        Py_DECREF(line);
        if (cpy == NULL)
            continue;

        size_t len = strlen(cpy);
        if ((len > 0) && (cpy[len - 1] == '\n'))
            cpy[len - 1] = '\0';

        Py_BEGIN_ALLOW_THREADS;
        PLUGIN_ERROR("%s", cpy);
        Py_END_ALLOW_THREADS;

        free(cpy);
    }

    Py_XDECREF(list);
    PyErr_Clear();
}

static int cpy_read_callback(user_data_t *data)
{
    cpy_callback_t *c = data->data;
    PyObject *ret;

    CPY_LOCK_THREADS
    ret = PyObject_CallFunctionObjArgs(c->callback, c->data, (void *)0); /* New reference. */
    if (ret == NULL) {
        cpy_log_exception("read callback");
    } else {
        Py_DECREF(ret);
    }
    CPY_RELEASE_THREADS
    if (ret == NULL)
        return 1;
    return 0;
}

static PyObject *cpy_create_metric(metric_t *m, metric_type_t type)
{
    PyObject *cpy_metric = NULL;
    Metric *metric = NULL;

    switch (type) {
    case METRIC_TYPE_UNKNOWN:
        if (m->value.unknown.type == UNKNOWN_FLOAT64) {
            PyObject *cpy_value = PyFloat_FromDouble(m->value.unknown.float64);
            cpy_metric = MetricUnknownDouble_New(cpy_value);
            Py_XDECREF(cpy_value);
            if (cpy_metric != NULL) {
                MetricUnknownDouble *unknown = (MetricUnknownDouble *) cpy_metric;
                metric = &unknown->metric;
            }
        } else if (m->value.unknown.type == UNKNOWN_INT64) {
            PyObject *cpy_value = PyLong_FromLongLong(m->value.unknown.int64);
            cpy_metric = MetricUnknownLong_New(cpy_value);
            Py_XDECREF(cpy_value);
            if (cpy_metric != NULL) {
                MetricUnknownLong *unknown = (MetricUnknownLong *) cpy_metric;
                metric = &unknown->metric;
            }
        }
        break;
    case METRIC_TYPE_GAUGE:
        if (m->value.gauge.type == GAUGE_FLOAT64) {
            PyObject *cpy_value = PyFloat_FromDouble(m->value.gauge.float64);
            cpy_metric = MetricGaugeDouble_New(cpy_value);
            Py_XDECREF(cpy_value);
            if (cpy_metric != NULL) {
                MetricGaugeDouble *gauge = (MetricGaugeDouble *) cpy_metric;
                metric = &gauge->metric;
            }
        } else if (m->value.gauge.type == GAUGE_INT64) {
            PyObject *cpy_value = PyLong_FromLongLong(m->value.gauge.int64);
            cpy_metric = MetricGaugeLong_New(cpy_value);
            Py_XDECREF(cpy_value);
            if (cpy_metric != NULL) {
                MetricGaugeLong *gauge = (MetricGaugeLong *) cpy_metric;
                metric = &gauge->metric;
            }
        }
        break;
    case METRIC_TYPE_COUNTER:
        if (m->value.counter.type == COUNTER_UINT64) {
            PyObject *cpy_value = PyLong_FromUnsignedLongLong(m->value.counter.uint64);
            cpy_metric = MetricCounterULong_New(cpy_value);
            Py_XDECREF(cpy_value);
            if (cpy_metric != NULL) {
                MetricCounterULong *counter = (MetricCounterULong *) cpy_metric;
                metric = &counter->metric;
            }
        } else if (m->value.counter.type == COUNTER_FLOAT64) {
            PyObject *cpy_value = PyFloat_FromDouble(m->value.counter.float64);
            cpy_metric = MetricCounterDouble_New(cpy_value);
            Py_XDECREF(cpy_value);
            if (cpy_metric != NULL) {
                MetricCounterDouble *counter = (MetricCounterDouble *) cpy_metric;
                metric = &counter->metric;
            }
        }
        break;
    case METRIC_TYPE_STATE_SET: {
        PyObject *dict_set = PyDict_New();
        if (dict_set != NULL) {
            for (size_t i = 0; i < m->value.state_set.num ; i++) {
                state_t *state_item = &m->value.state_set.ptr[i];
                PyDict_SetItemString(dict_set, state_item->name,
                                               state_item->enabled ? Py_True : Py_False);
            }
            cpy_metric = MetricStateSet_New(dict_set);
            if (cpy_metric != NULL) {
                MetricStateSet *set = (MetricStateSet *) cpy_metric;
                metric = &set->metric;
            }
            Py_DECREF(dict_set);
        }
    }   break;
    case METRIC_TYPE_INFO: {
        PyObject *dict_info = PyDict_New();
        if (dict_info != NULL) {
            if (m->value.info.num > 0) {
                for (size_t i = 0; i < m->value.info.num ; i++) {
                    label_pair_t *pair = &m->value.info.ptr[i];
                    PyObject *value = cpy_string_to_unicode_or_bytes(pair->value); /* New reference. */
                    PyDict_SetItemString(dict_info, pair->name, value);
                    Py_XDECREF(value);
                }
            }
            cpy_metric = MetricInfo_New(dict_info);
            if (cpy_metric != NULL) {
                MetricInfo *info = (MetricInfo *) cpy_metric;
                metric = &info->metric;
            }
            Py_DECREF(dict_info);
        }
    }   break;
    case METRIC_TYPE_SUMMARY: {
        PyObject *quantiles = PyList_New(0);
        if (quantiles != NULL) {
            for (size_t i = 0; i < m->value.summary->num ; i++) {
                summary_quantile_t *quantile = &m->value.summary->quantiles[i];
                PyObject *tuple = PyTuple_New(2); /* New reference. */
                if (tuple != NULL) {
                    PyObject *o_quantile = PyFloat_FromDouble(quantile->quantile);
                    if (o_quantile != NULL)
                        PyTuple_SET_ITEM(tuple, 0, o_quantile);
                    PyObject *o_value = PyFloat_FromDouble(quantile->value);
                    if (o_value != NULL)
                        PyTuple_SET_ITEM(tuple, 1, o_value);
                    PyList_Append(quantiles, tuple);
                    Py_DECREF(tuple);
                }
            }
            PyObject *cpy_sum = PyFloat_FromDouble(m->value.summary->sum);
            PyObject *cpy_count = PyLong_FromUnsignedLongLong(m->value.summary->count);
            cpy_metric = MetricSummary_New(cpy_sum, cpy_count, quantiles);
            Py_XDECREF(cpy_sum);
            Py_XDECREF(cpy_count);
            if (cpy_metric != NULL) {
                MetricSummary *summary = (MetricSummary *) cpy_metric;
                metric = &summary->metric;
            }
            Py_DECREF(quantiles);
        }
    }   break;
    case METRIC_TYPE_HISTOGRAM: {
        PyObject *buckets = PyList_New(0);
        if (buckets != NULL) {
            for (size_t i = 0; i < m->value.histogram->num ; i++) {
                histogram_bucket_t *bucket = &m->value.histogram->buckets[i];
                PyObject *tuple = PyTuple_New(2); /* New reference. */
                if (tuple != NULL) {
                    PyObject *o_counter = PyLong_FromUnsignedLongLong(bucket->counter);
                    if (o_counter != NULL)
                        PyTuple_SET_ITEM(tuple, 0, o_counter);
                    PyObject *o_maximum = PyFloat_FromDouble(bucket->maximum);
                    if (o_maximum != NULL)
                        PyTuple_SET_ITEM(tuple, 1, o_maximum);
                    PyList_Append(buckets, tuple);
                    Py_DECREF(tuple);
                }
            }
            PyObject *cpy_sum = PyFloat_FromDouble(m->value.histogram->sum);
            cpy_metric = MetricHistogram_New(cpy_sum, buckets);
            Py_XDECREF(cpy_sum);
            if (cpy_metric != NULL) {
                MetricHistogram *histogram = (MetricHistogram *) cpy_metric;
                metric = &histogram->metric;
            }
            Py_DECREF(buckets);
        }
    }   break;
    case METRIC_TYPE_GAUGE_HISTOGRAM: {
        PyObject *buckets = PyList_New(0);
        if (buckets != NULL) {
            for (size_t i = 0; i < m->value.histogram->num ; i++) {
                histogram_bucket_t *bucket = &m->value.histogram->buckets[i];
                PyObject *tuple = PyTuple_New(2); /* New reference. */
                if (tuple != NULL) {
                    PyObject *o_counter = PyLong_FromUnsignedLongLong(bucket->counter);
                    if (o_counter != NULL)
                        PyTuple_SET_ITEM(tuple, 0, o_counter);
                    PyObject *o_maximum = PyFloat_FromDouble(bucket->maximum);
                    if (o_maximum != NULL)
                        PyTuple_SET_ITEM(tuple, 1, o_maximum);
                    PyList_Append(buckets, tuple);
                    Py_DECREF(tuple);
                }
            }
            PyObject *cpy_sum = PyFloat_FromDouble(m->value.histogram->sum);
            cpy_metric = MetricGaugeHistogram_New(cpy_sum, buckets);
            Py_XDECREF(cpy_sum);
            if (cpy_metric != NULL) {
                MetricGaugeHistogram *gauge_histogram = (MetricGaugeHistogram *) cpy_metric;
                metric = &gauge_histogram->metric;
            }
            Py_DECREF(buckets);
        }
    }   break;
    }

    if (cpy_metric == NULL)
        return NULL;

    metric->time = CDTIME_T_TO_DOUBLE(m->time);
    metric->interval = CDTIME_T_TO_DOUBLE(m->interval);

    for (size_t i = 0; i < m->label.num ; i++) {
        label_pair_t *pair = &m->label.ptr[i];
        PyObject *value = cpy_string_to_unicode_or_bytes(pair->value); /* New reference. */
        PyDict_SetItemString(metric->labels, pair->name, value);
        Py_XDECREF(value);
    }

    return cpy_metric;
}

static int cpy_write_callback(const metric_family_t *fam, user_data_t *data)
{
    cpy_callback_t *c = data->data;
    PyObject *ret = NULL;
    if (fam->name == NULL)
        return 0;

    CPY_LOCK_THREADS
    PyObject *list = PyList_New(fam->metric.num); /* New reference. */
    if (list == NULL) {
        cpy_log_exception("write callback");
        CPY_RETURN_FROM_THREADS -1;
    }

    for (size_t i = 0; i < fam->metric.num; ++i) {
        metric_t *m = fam->metric.ptr + i;
        PyObject *item = cpy_create_metric(m, fam->type);
        if (item == NULL) {
            if (PyErr_Occurred() != NULL) {
                cpy_log_exception("metric building for write callback");
            } else {
                Py_BEGIN_ALLOW_THREADS;
                PLUGIN_ERROR("Unable to build the metric %zu of the family '%s' "
                             "for the write callback.", i, fam->name);
                Py_END_ALLOW_THREADS;
            }
            Py_DECREF(list);
            CPY_RETURN_FROM_THREADS -1;
        }

        PyList_SetItem(list, i, item);

        if (PyErr_Occurred() != NULL) {
            cpy_log_exception("metric building for write callback");
            Py_DECREF(list);
            CPY_RETURN_FROM_THREADS -1;
        }
    }

    PyObject *fam_name = cpy_string_to_unicode_or_bytes(fam->name); /* New reference. */
    PyObject *cpy_type = PyLong_FromLong(fam->type);
    PyObject *pymf = MetricFamily_New(fam_name, cpy_type); /* New reference. */
    Py_XDECREF(fam_name);
    Py_XDECREF(cpy_type);
    if (pymf == NULL) {
        cpy_log_exception("failed to create MetricFamily object");
        Py_DECREF(list);
        CPY_RETURN_FROM_THREADS -1;
    }

    MetricFamily *mf = (MetricFamily *)pymf;

    if (fam->help != NULL)
        mf->help = cpy_string_to_unicode_or_bytes(fam->help); /* New reference. */
    if (fam->unit != NULL)
        mf->unit = cpy_string_to_unicode_or_bytes(fam->unit); /* New reference. */

    Py_CLEAR(mf->metrics);
    mf->metrics = list;

    ret = PyObject_CallFunctionObjArgs(c->callback, mf, c->data, (void *)0); /* New reference. */
    Py_XDECREF(pymf);
    if (ret == NULL) {
        cpy_log_exception("write callback");
        CPY_RETURN_FROM_THREADS -1;
    } else {
        Py_DECREF(ret);
    }

    CPY_RELEASE_THREADS
    return 0;
}

static int cpy_notification_callback(const notification_t *notification, user_data_t *data)
{
    cpy_callback_t *c = data->data;
    PyObject *ret;

    if (notification->name == NULL)
        return 0;

    CPY_LOCK_THREADS

    PyObject *name = cpy_string_to_unicode_or_bytes(notification->name); /* New reference. */

    PyObject *notify = Notification_New(name); /* New reference. */
    if (notify == NULL) {
        cpy_log_exception("failed to create Notification object");
        CPY_RETURN_FROM_THREADS -1;
    }

    Notification *n = (Notification *)notify;

    Py_XDECREF(name);

    n->severity = notification->severity;
    n->time = CDTIME_T_TO_DOUBLE(notification->time);

    if (n->labels == NULL)
        n->labels = PyDict_New(); /* New reference. */
    if (n->labels != NULL) {
        for (size_t i = 0; i < notification->label.num ; i++) {
            label_pair_t *pair = &notification->label.ptr[i];
            PyObject *value = cpy_string_to_unicode_or_bytes(pair->value); /* New reference. */
            PyDict_SetItemString(n->labels, pair->name, value);
            Py_XDECREF(value);
        }
    }

    if (n->annotations == NULL)
        n->annotations = PyDict_New(); /* New reference. */
    if (n->annotations != NULL) {
        for (size_t i = 0; i < notification->annotation.num ; i++) {
            label_pair_t *pair = &notification->annotation.ptr[i];
            PyObject *value = cpy_string_to_unicode_or_bytes(pair->value); /* New reference. */
            PyDict_SetItemString(n->annotations, pair->name, value);
            Py_XDECREF(value);
        }
    }

    ret = PyObject_CallFunctionObjArgs(c->callback, n, c->data, (void *)0); /* New reference. */
    Py_XDECREF(notify);
    if (ret == NULL) {
        cpy_log_exception("notification callback");
        CPY_RETURN_FROM_THREADS -1;
    } else {
        Py_DECREF(ret);
    }

    CPY_RELEASE_THREADS
    return 0;
}

static void cpy_log_callback(const log_msg_t *msg, user_data_t *data)
{
    cpy_callback_t *c = data->data;

    CPY_LOCK_THREADS

    PyObject *exc_type, *exc_value, *exc_traceback;
    PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);

    PyObject *text = cpy_string_to_unicode_or_bytes(msg->msg); /* New reference. */
    PyObject *ret;

    if (c->data == NULL) {
        ret = PyObject_CallFunction( c->callback, "iN", msg->severity, text);
        /* New reference. Steals a reference from "text". */
    } else {
        ret = PyObject_CallFunction(c->callback, "iNO", msg->severity, text, c->data);
        /* New reference. Steals a reference from "text". */
    }

    if (ret == NULL) {
        /* Do not trigger a log callback because a log callback failed.
         * PyErr_Print() can not be used here: it calls Py_Exit() on SystemExit. */
        PyErr_WriteUnraisable(c->callback);
        /* In case someone wanted to be clever, replaced stderr and failed at that. */
        PyErr_Clear();
    } else {
        Py_DECREF(ret);
    }

    PyErr_Restore(exc_type, exc_value, exc_traceback);

    CPY_RELEASE_THREADS
}

static PyObject *cpy_register_generic(cpy_callback_t **list_head, PyObject *args, PyObject *kwds)
{
    char buf[512];
    char *name = NULL;
    PyObject *callback = NULL, *data = NULL;
    static char *kwlist[] = {"callback", "data", "name", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O|Oet", kwlist, &callback, &data, NULL, &name) == 0)
        return NULL;
    if (PyCallable_Check(callback) == 0) {
        PyMem_Free(name);
        PyErr_SetString(PyExc_TypeError, "callback needs a be a callable object.");
        return NULL;
    }
    cpy_build_name(buf, sizeof(buf), callback, name);
    PyMem_Free(name);


    cpy_callback_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    c->name = strdup(buf);
    if (c->name == NULL) {
        free(c);
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(callback);
    c->callback = callback;
    Py_XINCREF(data);
    c->data = data;

    c->next = *list_head;
    ++cpy_num_callbacks;
    *list_head = c;

    char fullname[512];
    ssnprintf(fullname, sizeof(fullname), "%s/%s", "python", buf);
    return cpy_string_to_unicode_or_bytes(fullname);
}

static PyObject *cpy_register_config(__attribute__((unused)) PyObject *self,
                                     PyObject *args, PyObject *kwds)
{
    return cpy_register_generic(&cpy_config_callbacks, args, kwds);
}

static PyObject *cpy_register_init(__attribute__((unused)) PyObject *self,
                                   PyObject *args, PyObject *kwds)
{
    return cpy_register_generic(&cpy_init_callbacks, args, kwds);
}

typedef int reg_function_t(const char *group, const char *name, void *callback, void *data);

static PyObject *cpy_register_generic_userdata(void *reg, void *handler,
                                               PyObject *args, PyObject *kwds)
{
    char buf[512];
    reg_function_t *register_function = (reg_function_t *)reg;
    char *name = NULL;
    PyObject *callback = NULL, *data = NULL;
    static char *kwlist[] = {"callback", "data", "name", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O|Oet", kwlist, &callback, &data,
                                                                    NULL, &name) == 0)
        return NULL;
    if (PyCallable_Check(callback) == 0) {
        PyMem_Free(name);
        PyErr_SetString(PyExc_TypeError, "callback needs a be a callable object.");
        return NULL;
    }
    cpy_build_name(buf, sizeof(buf), callback, name);
    PyMem_Free(name);

    cpy_callback_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    c->name = strdup(buf);
    if (c->name == NULL) {
        free(c);
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(callback);
    c->callback = callback;
    Py_XINCREF(data);
    c->data = data;
    c->next = NULL;

    ++cpy_num_callbacks;

    register_function("python", buf, handler,
                      &(user_data_t){.data = c, .free_func = cpy_destroy_user_data});

    char fullname[512];
    ssnprintf(fullname, sizeof(fullname), "%s/%s", "python", buf);
    return cpy_string_to_unicode_or_bytes(fullname);
}

static PyObject *cpy_register_read(__attribute__((unused)) PyObject *self,
                                   PyObject *args, PyObject *kwds)
{
    char buf[512];
    double interval = 0;
    char *name = NULL;
    PyObject *callback = NULL, *data = NULL;
    static char *kwlist[] = {"callback", "interval", "data", "name", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O|dOet", kwlist, &callback,
                                                 &interval, &data, NULL, &name) == 0)
        return NULL;
    if (PyCallable_Check(callback) == 0) {
        PyMem_Free(name);
        PyErr_SetString(PyExc_TypeError, "callback needs a be a callable object.");
        return NULL;
    }
    cpy_build_name(buf, sizeof(buf), callback, name);
    PyMem_Free(name);

    cpy_callback_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    c->name = strdup(buf);
    if (c->name == NULL) {
        free(c);
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(callback);
    c->callback = callback;
    Py_XINCREF(data);
    c->data = data;
    c->next = NULL;

    ++cpy_num_callbacks;

    if (interval < 0 || isnan(interval))
        interval = 0;

    int status = plugin_register_complex_read("python", buf, cpy_read_callback,
                                              DOUBLE_TO_CDTIME_T(interval),
                                               &(user_data_t){ .data = c, 
                                                               .free_func = cpy_destroy_user_data });
    if (status != 0) {
        PyErr_SetString(PyExc_TypeError, "plugin_register_complex_read failed.");
        return NULL;
    }
    
    char fullname[512];
    ssnprintf(fullname, sizeof(fullname), "%s/%s", "python", buf);
    return cpy_string_to_unicode_or_bytes(fullname);
}

static PyObject *cpy_register_log(__attribute__((unused)) PyObject *self,
                                  PyObject *args, PyObject *kwds)
{
    return cpy_register_generic_userdata((void *)plugin_register_log,
                                         (void *)cpy_log_callback, args, kwds);
}

static PyObject *cpy_register_write(__attribute__((unused)) PyObject *self,
                                    PyObject *args, PyObject *kwds)
{
    char buf[512];
    char *name = NULL;
    PyObject *callback = NULL, *data = NULL;
    static char *kwlist[] = {"callback", "data", "name", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O|Oet", kwlist, &callback, &data,
                                                                    NULL, &name) == 0)
        return NULL;

    if (PyCallable_Check(callback) == 0) {
        PyMem_Free(name);
        PyErr_SetString(PyExc_TypeError, "callback needs a be a callable object.");
        return NULL;
    }

    cpy_build_name(buf, sizeof(buf), callback, name);
    PyMem_Free(name);

    cpy_callback_t *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    c->name = strdup(buf);
    if (c->name == NULL) {
        free(c);
        PyErr_NoMemory();
        return NULL;
    }

    Py_INCREF(callback);
    c->callback = callback;
    Py_XINCREF(data);
    c->data = data;
    c->next = NULL;

    ++cpy_num_callbacks;

    int status = plugin_register_write("python", buf, cpy_write_callback, NULL, 0, 0,
                                       &(user_data_t){.data = c,
                                                      .free_func = cpy_destroy_user_data});
    if (status != 0) {
        PyErr_SetString(PyExc_TypeError, "plugin_register_write failed.");
        return NULL;
    }

    char fullname[512];
    ssnprintf(fullname, sizeof(fullname), "%s/%s", "python", buf);
    return cpy_string_to_unicode_or_bytes(fullname);
}

static PyObject *cpy_register_notification(__attribute__((unused)) PyObject *self,
                                           PyObject *args, PyObject *kwds)
{
    return cpy_register_generic_userdata((void *)plugin_register_notification,
                                         (void *)cpy_notification_callback, args, kwds);
}

static PyObject *cpy_register_shutdown(__attribute__((unused)) PyObject *self,
                                       PyObject *args, PyObject *kwds)
{
    return cpy_register_generic(&cpy_shutdown_callbacks, args, kwds);
}

static PyObject *cpy_error(__attribute__((unused)) PyObject *self, PyObject *args)
{
    char *text;
    if (PyArg_ParseTuple(args, "et", NULL, &text) == 0)
        return NULL;
    Py_BEGIN_ALLOW_THREADS;
    plugin_log(LOG_ERR, NULL, 0, NULL, "%s", text);
    Py_END_ALLOW_THREADS;
    PyMem_Free(text);
    Py_RETURN_NONE;
}

static PyObject *cpy_warning(__attribute__((unused)) PyObject *self, PyObject *args)
{
    char *text;
    if (PyArg_ParseTuple(args, "et", NULL, &text) == 0)
        return NULL;
    Py_BEGIN_ALLOW_THREADS;
    plugin_log(LOG_WARNING, NULL, 0, NULL, "%s", text);
    Py_END_ALLOW_THREADS;
    PyMem_Free(text);
    Py_RETURN_NONE;
}

static PyObject *cpy_notice(__attribute__((unused)) PyObject *self, PyObject *args)
{
    char *text;
    if (PyArg_ParseTuple(args, "et", NULL, &text) == 0)
        return NULL;
    Py_BEGIN_ALLOW_THREADS;
    plugin_log(LOG_NOTICE, NULL, 0, NULL, "%s", text);
    Py_END_ALLOW_THREADS;
    PyMem_Free(text);
    Py_RETURN_NONE;
}

static PyObject *cpy_info(__attribute__((unused)) PyObject *self, PyObject *args)
{
    char *text;
    if (PyArg_ParseTuple(args, "et", NULL, &text) == 0)
        return NULL;
    Py_BEGIN_ALLOW_THREADS;
    plugin_log(LOG_INFO, NULL, 0, NULL, "%s", text);
    Py_END_ALLOW_THREADS;
    PyMem_Free(text);
    Py_RETURN_NONE;
}

static PyObject *cpy_debug(__attribute__((unused)) PyObject *self, PyObject *args)
{
#ifdef NCOLLECTD_DEBUG
    char *text;
    if (PyArg_ParseTuple(args, "et", NULL, &text) == 0)
        return NULL;
    Py_BEGIN_ALLOW_THREADS;
    plugin_log(LOG_DEBUG, NULL, 0, NULL, "%s", text);
    Py_END_ALLOW_THREADS;
    PyMem_Free(text);
#else
    (void)args;
#endif
    Py_RETURN_NONE;
}

static PyObject *cpy_unregister_generic(cpy_callback_t **list_head, PyObject *arg, const char *desc)
{
    char buf[512];
    cpy_callback_t *prev = NULL, *tmp;

    Py_INCREF(arg);
    const char *name = cpy_unicode_or_bytes_to_string(&arg);
    if (name == NULL) {
        PyErr_Clear();
        if (!PyCallable_Check(arg)) {
            PyErr_SetString(PyExc_TypeError, "This function needs a string or a "
                                             "callable object as its only parameter.");
            Py_DECREF(arg);
            return NULL;
        }
        cpy_build_name(buf, sizeof(buf), arg, NULL);
        name = buf;
    }
    
    const char *short_name = name;
    if (strncmp(name, "python/", strlen("python/")) == 0)
        short_name = name + strlen("python/");

    for (tmp = *list_head; tmp; prev = tmp, tmp = tmp->next)
        if (strcmp(short_name, tmp->name) == 0)
            break;

    if (tmp == NULL) {
        PyErr_Format(PyExc_RuntimeError, "Unable to unregister %s callback '%s'.", desc, name);
        Py_DECREF(arg);
        return NULL;
    }

    Py_DECREF(arg);

    /* Yes, this is actually safe. To call this function the caller has to
     * hold the GIL. Well, safe as long as there is only one GIL anyway ... */
    if (prev == NULL)
        *list_head = tmp->next;
    else
        prev->next = tmp->next;

    cpy_destroy_user_data(tmp);

    Py_RETURN_NONE;
}

static void cpy_unregister_list(cpy_callback_t **list_head)
{
    cpy_callback_t *cur, *next;
    for (cur = *list_head; cur; cur = next) {
        next = cur->next;
        cpy_destroy_user_data(cur);
    }
    *list_head = NULL;
}

typedef int cpy_unregister_function_t(const char *name);

static PyObject * cpy_unregister_generic_userdata(cpy_unregister_function_t *unreg, PyObject *arg,
                                                  const char *desc)
{
    char buf[512];

    Py_INCREF(arg);
    const char *name = cpy_unicode_or_bytes_to_string(&arg);
    if (name == NULL) {
        PyErr_Clear();
        if (!PyCallable_Check(arg)) {
            PyErr_SetString(PyExc_TypeError, "This function needs a string or a "
                                             "callable object as its only parameter.");
            Py_DECREF(arg);
            return NULL;
        }
        cpy_build_name(buf, sizeof(buf), arg, NULL);
        name = buf;
    }
    
    char fullname[512];
    if (strncmp(name, "python/", strlen("python/")) != 0) {
        ssnprintf(fullname, sizeof(fullname), "python/%s", name);
        name = fullname;
    }

    int status = 0;

    Py_BEGIN_ALLOW_THREADS;
    status = unreg(name);
    Py_END_ALLOW_THREADS;

    if (status == 0) {
        Py_DECREF(arg);
        Py_RETURN_NONE;
    }

    PyErr_Format(PyExc_RuntimeError, "Unable to unregister %s callback '%s'.", desc, name);
    Py_DECREF(arg);
    return NULL;
}

static PyObject *cpy_unregister_log(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic_userdata(plugin_unregister_log, arg, "log");
}

static PyObject *cpy_unregister_init(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic(&cpy_init_callbacks, arg, "init");
}

static PyObject *cpy_unregister_config(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic(&cpy_config_callbacks, arg, "config");
}

static PyObject *cpy_unregister_read(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic_userdata(plugin_unregister_read, arg, "read");
}

static PyObject *cpy_unregister_write(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic_userdata(plugin_unregister_write, arg, "write");
}

static PyObject *cpy_unregister_notification(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic_userdata(plugin_unregister_notification, arg, "notification");
}

static PyObject *cpy_unregister_shutdown(__attribute__((unused)) PyObject *self, PyObject *arg)
{
    return cpy_unregister_generic(&cpy_shutdown_callbacks, arg, "shutdown");
}

static PyMethodDef cpy_methods[] = {
    {"debug",                   cpy_debug,                        METH_VARARGS, log_doc},
    {"info",                    cpy_info,                         METH_VARARGS, log_doc},
    {"notice",                  cpy_notice,                       METH_VARARGS, log_doc},
    {"warning",                 cpy_warning,                      METH_VARARGS, log_doc},
    {"error",                   cpy_error,                        METH_VARARGS, log_doc},
    {"register_log",            (PyCFunction)cpy_register_log,    METH_VARARGS | METH_KEYWORDS, reg_log_doc},
    {"register_init",           (PyCFunction)cpy_register_init,   METH_VARARGS | METH_KEYWORDS, reg_init_doc},
    {"register_config",         (PyCFunction)cpy_register_config, METH_VARARGS | METH_KEYWORDS, reg_config_doc},
    {"register_read",           (PyCFunction)cpy_register_read,   METH_VARARGS | METH_KEYWORDS, reg_read_doc},
    {"register_write",          (PyCFunction)cpy_register_write,  METH_VARARGS | METH_KEYWORDS, reg_write_doc},
    {"register_notification",   (PyCFunction)cpy_register_notification, METH_VARARGS | METH_KEYWORDS, reg_notification_doc},
    {"register_shutdown",       (PyCFunction)cpy_register_shutdown,     METH_VARARGS | METH_KEYWORDS, reg_shutdown_doc},
    {"unregister_log",          cpy_unregister_log,          METH_O, unregister_doc},
    {"unregister_init",         cpy_unregister_init,         METH_O, unregister_doc},
    {"unregister_config",       cpy_unregister_config,       METH_O, unregister_doc},
    {"unregister_read",         cpy_unregister_read,         METH_O, unregister_doc},
    {"unregister_write",        cpy_unregister_write,        METH_O, unregister_doc},
    {"unregister_notification", cpy_unregister_notification, METH_O, unregister_doc},
    {"unregister_shutdown",     cpy_unregister_shutdown,     METH_O, unregister_doc},
    {0, 0, 0, 0}
};

static void cpy_call_callback_list(cpy_callback_t **list_head, const char *context)
{
    size_t num = 0;
    for (cpy_callback_t *c = *list_head; c != NULL; c = c->next)
        num++;

    if (num == 0)
        return;

    struct {
        PyObject *callback;
        PyObject *data;
    } *snapshot = calloc(num, sizeof(*snapshot));
    if (snapshot == NULL) {
        PyErr_NoMemory();
        cpy_log_exception(context);
        return;
    }

    size_t n = 0;
    for (cpy_callback_t *c = *list_head; (c != NULL) && (n < num); c = c->next, n++) {
        Py_INCREF(c->callback);
        snapshot[n].callback = c->callback;
        Py_XINCREF(c->data);
        snapshot[n].data = c->data;
    }

    for (size_t i = 0; i < n; i++) {
        /* Skip the callbacks unregistered by a previous callback. */
        bool registered = false;
        for (cpy_callback_t *c = *list_head; c != NULL; c = c->next) {
            if ((c->callback == snapshot[i].callback) && (c->data == snapshot[i].data)) {
                registered = true;
                break;
            }
        }
        if (!registered)
            continue;

        PyObject *ret = PyObject_CallFunctionObjArgs(snapshot[i].callback, snapshot[i].data,
                                                     (void *)0); /* New reference. */
        if (ret == NULL)
            cpy_log_exception(context);
        else
            Py_DECREF(ret);
    }

    for (size_t i = 0; i < n; i++) {
        Py_DECREF(snapshot[i].callback);
        Py_XDECREF(snapshot[i].data);
    }

    free(snapshot);
}

static int cpy_shutdown(void)
{
    if (!state) {
        printf("================================================================\n");
        printf("ncollectd shutdown while running an interactive session.\n");
        printf("This will probably leave your terminal in a mess.\n");
        printf("Run the command 'reset' to get it back into a usable state.\n");
        printf("You can press Ctrl+D in the interactive session to\n");
        printf("close ncollectd and avoid this problem in the future.\n");
        printf("================================================================\n");
    }

    CPY_LOCK_THREADS

    cpy_call_callback_list(&cpy_shutdown_callbacks, "shutdown callback");
    PyErr_Print();

    Py_BEGIN_ALLOW_THREADS;
    cpy_unregister_list(&cpy_config_callbacks);
    cpy_unregister_list(&cpy_init_callbacks);
    cpy_unregister_list(&cpy_shutdown_callbacks);
    Py_END_ALLOW_THREADS;

    cpy_shutdown_triggered = 1;

    PyGC_Collect(); // FIXME

    if (cpy_num_callbacks == 0) {
        Py_Finalize();
        return 0;
    }

    CPY_RELEASE_THREADS
    return 0;
}

static void *cpy_interactive(void *pipefd)
{
    PyOS_sighandler_t cur_sig;

    PyGILState_STATE gilstate = PyGILState_Ensure();


    /* Signal handler in a plugin? Bad stuff, but the best way to
     * handle it I guess. In an interactive session people will
     * press Ctrl+C at some time, which will generate a SIGINT.
     * This will cause ncollectd to shutdown, thus killing the
     * interactive interpreter, and leaving the terminal in a
     * mess. Chances are, this isn't what the user wanted to do.
     *
     * So this is the plan:
     * 1. Restore Python's own signal handler
     * 2. Tell Python we just forked so it will accept this thread
     *      as the main one. No version of Python will ever handle
     *      interrupts anywhere but in the main thread.
     * 3. After the interactive loop is done, restore ncollectd's
     *      SIGINT handler.
     * 4. Raise SIGINT for a clean shutdown. The signal is sent to
     *      the main thread to ensure it wakes up the main interval
     *      sleep so that ncollectd shuts down immediately not in 10
     *      seconds.
     *
     * This will make sure that SIGINT won't kill ncollectd but
     * still interrupt syscalls like sleep and pause. */

    if (PyImport_ImportModule("readline") == NULL) {
        /* This interactive session will suck. */
        cpy_log_exception("interactive session init");
    }
    cur_sig = PyOS_setsig(SIGINT, python_sigint_handler);
#if PY_VERSION_HEX < 0x03090000
    PyEval_InitThreads();
#else
    Py_Initialize();
#endif
    close(*(int *)pipefd);
    PyRun_InteractiveLoop(stdin, "<stdin>");
    PyOS_setsig(SIGINT, cur_sig);
    PyErr_Print();

    PyGILState_Release(gilstate);

    PLUGIN_NOTICE("Interactive interpreter exited, stopping ncollectd ...");
    pthread_kill(main_thread, SIGINT);

    return NULL;
}

static int cpy_init(void)
{
    int pipefd[2];
    char buf;
    static pthread_t thread;

    if (!Py_IsInitialized()) {
        PLUGIN_WARNING("Plugin loaded but not configured.");
        plugin_unregister_shutdown("python");
        Py_Finalize();
        return 0;
    }
    
    if (config_pid != getpid()) {
#if PY_VERSION_HEX < 0x03070000
        PyOS_AfterFork();
#else
        PyOS_AfterFork_Child();
#endif
    }

    main_thread = pthread_self();
    if (do_interactive) {
        if (pipe(pipefd)) {
            PLUGIN_ERROR(" Unable to create pipe.");
            return 1;
        }

        state = PyEval_SaveThread();

        if (plugin_thread_create(&thread, cpy_interactive, pipefd + 1, "python interpreter")) {
            close(pipefd[0]);
            close(pipefd[1]);
            PyEval_RestoreThread(state);
            state = NULL;
            PLUGIN_ERROR(" Error creating thread for interactive interpreter.");
            /* Leave the initialized interpreter detached for shutdown. */
            state = PyEval_SaveThread();
            return 1;
        }
        if (read(pipefd[0], &buf, 1)) {
            //FIXME
        }
        (void)close(pipefd[0]);
    } else {
#if PY_VERSION_HEX < 0x03090000
        PyEval_InitThreads();
#else
        Py_Initialize();
#endif
        state = PyEval_SaveThread();
    }
    CPY_LOCK_THREADS
    cpy_call_callback_list(&cpy_init_callbacks, "init callback");
    CPY_RELEASE_THREADS

    return 0;
}

static PyObject *cpy_config_to_pyconfig(config_item_t *ci, PyObject *parent)
{
    if (parent == NULL)
        parent = Py_None;

    PyObject *values = PyTuple_New(ci->values_num); /* New reference. */
    if (values == NULL)
        return NULL;

    for (int i = 0; i < ci->values_num; ++i) {
        switch(ci->values[i].type) {
        case CONFIG_TYPE_STRING:
        case CONFIG_TYPE_REGEX:
            PyTuple_SET_ITEM(values, i, cpy_string_to_unicode_or_bytes(ci->values[i].value.string));
            break;
        case CONFIG_TYPE_NUMBER:
            PyTuple_SET_ITEM(values, i, PyFloat_FromDouble(ci->values[i].value.number));
            break;
        case CONFIG_TYPE_BOOLEAN:
            PyTuple_SET_ITEM(values, i, PyBool_FromLong(ci->values[i].value.boolean));
            break;
        }
    }

    PyObject *tmp = cpy_string_to_unicode_or_bytes(ci->key);
    PyObject *item = PyObject_CallFunction((void *)&ConfigType, "OOOO", tmp, parent, values, Py_None);
    Py_XDECREF(tmp);
    Py_XDECREF(values);
    if (item == NULL)
        return NULL;

    PyObject *children = PyTuple_New(ci->children_num); /* New reference. */
    if (children == NULL) {
        Py_XDECREF(item);
        return NULL;
    }

    for (int i = 0; i < ci->children_num; ++i) {
        PyObject *config = cpy_config_to_pyconfig(ci->children + i, item);
        if (config == NULL) {
            Py_XDECREF(children);
            Py_XDECREF(item);
            return NULL;
        }
        PyTuple_SET_ITEM(children, i, config);
    }

    tmp = ((Config *)item)->children;
    ((Config *)item)->children = children;
    Py_XDECREF(tmp);

    return item;
}

#ifdef IS_PY3K
static struct PyModuleDef ncollectdmodule = {
    PyModuleDef_HEAD_INIT,
    "ncollectd",                         /* name of module */
    "The python interface to ncollectd", /* module documentation, may be NULL */
    -1,
    cpy_methods
};

PyMODINIT_FUNC PyInit_ncollectd(void)
{
    return PyModule_Create(&ncollectdmodule);
}
#endif

static int cpy_init_python(void)
{
    PyObject *sys, *errordict;
    PyObject *module;

    config_pid = getpid();

#ifdef IS_PY3K
    /* Add a builtin module, before Py_Initialize */
    PyImport_AppendInittab("ncollectd", PyInit_ncollectd);
#endif

    /* Chances are the current signal handler is already SIG_DFL, but let's make sure.
     * Python only installs its own SIGINT handler if the current one is SIG_DFL. */
    PyOS_sighandler_t cur_sig = PyOS_setsig(SIGINT, SIG_DFL);

#if PY_VERSION_ATLEAST(3, 8)
    PyConfig config = {0};
    PyConfig_InitPythonConfig(&config);
    PyConfig_SetBytesArgv(&config, 1, (char *[]){""});
    PyStatus status = Py_InitializeFromConfig(&config);
    if (PyStatus_IsError(status)) {
        PyOS_setsig(SIGINT, cur_sig);
        PLUGIN_ERROR("python initialization: Py_InitializeFromConfig(): %s", status.err_msg);
        return 1;
    } else if (PyStatus_IsExit(status)) {
        PyOS_setsig(SIGINT, cur_sig);
        PLUGIN_ERROR("python initialization: Py_InitializeFromConfig() returned exit code %d",
                     status.exitcode);
        return 1;
    }
    PyConfig_Clear(&config);
#else
    Py_Initialize();
#endif

     /* Keep the Python SIGINT handler for the interactive session and restore the old one. */
    python_sigint_handler = PyOS_setsig(SIGINT, cur_sig);

    if (PyType_Ready(&ConfigType) == -1) {
        cpy_log_exception("python initialization: ConfigType");
        return 1;
    }

    if (PyType_Ready(&MetricType) == -1) {
        cpy_log_exception("python initialization: MetricType");
        return 1;
    }

    MetricUnknownDoubleType.tp_base = &MetricType;
    if (PyType_Ready(&MetricUnknownDoubleType) == -1) {
        cpy_log_exception("python initialization: MetricUnknownDoubleType");
        return 1;
    }

    MetricUnknownLongType.tp_base = &MetricType;
    if (PyType_Ready(&MetricUnknownLongType) == -1) {
        cpy_log_exception("python initialization: MetricUnknownLongType");
        return 1;
    }

    MetricGaugeDoubleType.tp_base = &MetricType;
    if (PyType_Ready(&MetricGaugeDoubleType) == -1) {
        cpy_log_exception("python initialization: MetricGaugeDoubleType");
        return 1;
    }

    MetricGaugeLongType.tp_base = &MetricType;
    if (PyType_Ready(&MetricGaugeLongType) == -1) {
        cpy_log_exception("python initialization: MetricGaugeLongType");
        return 1;
    }

    MetricCounterULongType.tp_base = &MetricType;
    if (PyType_Ready(&MetricCounterULongType) == -1) {
        cpy_log_exception("python initialization: MetricCounterULongType");
        return 1;
    }

    MetricCounterDoubleType.tp_base = &MetricType;
    if (PyType_Ready(&MetricCounterDoubleType) == -1) {
        cpy_log_exception("python initialization: MetricCounterDoubleType");
        return 1;
    }

    MetricStateSetType.tp_base = &MetricType;
    if (PyType_Ready(&MetricStateSetType) == -1) {
        cpy_log_exception("python initialization: MetricStateSetType");
        return 1;
    }

    MetricInfoType.tp_base = &MetricType;
    if (PyType_Ready(&MetricInfoType) == -1) {
        cpy_log_exception("python initialization: MetricInfoType");
        return 1;
    }

    MetricSummaryType.tp_base = &MetricType;
    if (PyType_Ready(&MetricSummaryType) == -1) {
        cpy_log_exception("python initialization: MetricSummaryType");
        return 1;
    }

    MetricHistogramType.tp_base = &MetricType;
    if (PyType_Ready(&MetricHistogramType) == -1) {
        cpy_log_exception("python initialization: MetricHistogramType");
        return 1;
    }

    MetricGaugeHistogramType.tp_base = &MetricType;
    if (PyType_Ready(&MetricGaugeHistogramType) == -1) {
        cpy_log_exception("python initialization: MetricGaugeHistogramType");
        return 1;
    }

    if (PyType_Ready(&MetricFamilyType) == -1) {
        cpy_log_exception("python initialization: MetricFamilyType");
        return 1;
    }

    if (PyType_Ready(&NotificationType) == -1) {
        cpy_log_exception("python initialization: NotificationType");
        return 1;
    }

    errordict = PyDict_New();
    PyDict_SetItemString(errordict, "__doc__",
                         cpy_string_to_unicode_or_bytes(NCollectdError_doc)); /* New reference. */
    NCollectdError = PyErr_NewException("ncollectd.NCollectdError", NULL, errordict);
    sys = PyImport_ImportModule("sys"); /* New reference. */
    if (sys == NULL) {
        cpy_log_exception("python initialization");
        return 1;
    }
    sys_path = PyObject_GetAttrString(sys, "path"); /* New reference. */
    Py_DECREF(sys);
    if (sys_path == NULL) {
        cpy_log_exception("python initialization");
        return 1;
    }

#if PY_VERSION_ATLEAST(3, 8)
    /* no op : Py_InitializeFromConfig() does not prepend it. */
#elif defined(IS_PY3K)
    wchar_t *argv = L"";
    PySys_SetArgvEx(1, &argv, 0);
#else
    PyList_SetSlice(sys_path, 0, 1, NULL);
    char *argv = "";
    PySys_SetArgv(1, &argv);
#endif

#ifdef IS_PY3K
    module = PyImport_ImportModule("ncollectd");
#else
    module = Py_InitModule("ncollectd", cpy_methods); /* Borrowed reference. */
#endif
    if (module == NULL) {
        cpy_log_exception("Import module ncollectd failed");
        return 1;
    }

    Py_INCREF(&ConfigType);
    if (PyModule_AddObject(module, "Config", (void *)&ConfigType) != 0) {
        Py_DECREF(&ConfigType);
        goto error;
    }

    Py_INCREF(&MetricType);
    if (PyModule_AddObject(module, "Metric", (void *)&MetricType) != 0) {
        Py_DECREF(&MetricType);
        goto error;
    }

    Py_INCREF(&MetricUnknownDoubleType);
    if (PyModule_AddObject(module, "MetricUnknownDouble", (void *)&MetricUnknownDoubleType) != 0) {
        Py_DECREF(&MetricUnknownDoubleType);
        goto error;
    }

    Py_INCREF(&MetricUnknownLongType);
    if (PyModule_AddObject(module, "MetricUnknownLong", (void *)&MetricUnknownLongType) != 0) {
        Py_DECREF(&MetricUnknownLongType);
        goto error;
    }

    Py_INCREF(&MetricGaugeDoubleType);
    if (PyModule_AddObject(module, "MetricGaugeDouble", (void *)&MetricGaugeDoubleType) != 0) {
        Py_DECREF(&MetricGaugeDoubleType);
        goto error;
    }

    Py_INCREF(&MetricGaugeLongType);
    if (PyModule_AddObject(module, "MetricGaugeLong", (void *)&MetricGaugeLongType) != 0) {
        Py_DECREF(&MetricGaugeLongType);
        goto error;
    }

    Py_INCREF(&MetricCounterULongType);
    if (PyModule_AddObject(module, "MetricCounterULong", (void *)&MetricCounterULongType) != 0) {
        Py_DECREF(&MetricCounterULongType);
        goto error;
    }

    Py_INCREF(&MetricCounterDoubleType);
    if (PyModule_AddObject(module, "MetricCounterDouble", (void *)&MetricCounterDoubleType) != 0) {
        Py_DECREF(&MetricCounterDoubleType);
        goto error;
    }

    Py_INCREF(&MetricStateSetType);
    if (PyModule_AddObject(module, "MetricStateSet", (void *)&MetricStateSetType) != 0) {
        Py_DECREF(&MetricStateSetType);
        goto error;
    }

    Py_INCREF(&MetricInfoType);
    if (PyModule_AddObject(module, "MetricInfo", (void *)&MetricInfoType) != 0) {
        Py_DECREF(&MetricInfoType);
        goto error;
    }

    Py_INCREF(&MetricSummaryType);
    if (PyModule_AddObject(module, "MetricSummary", (void *)&MetricSummaryType) != 0) {
        Py_DECREF(&MetricSummaryType);
        goto error;
    }

    Py_INCREF(&MetricHistogramType);
    if (PyModule_AddObject(module, "MetricHistogram", (void *)&MetricHistogramType) != 0) {
        Py_DECREF(&MetricHistogramType);
        goto error;
    }

    Py_INCREF(&MetricGaugeHistogramType);
    if (PyModule_AddObject(module, "MetricGaugeHistogram", (void *)&MetricGaugeHistogramType) != 0) {
        Py_DECREF(&MetricGaugeHistogramType);
        goto error;
    }

    Py_INCREF(&MetricFamilyType);
    if (PyModule_AddObject(module, "MetricFamily", (void *)&MetricFamilyType) != 0) {
        Py_DECREF(&MetricFamilyType);
        goto error;
    }

    Py_INCREF(&NotificationType);
    if (PyModule_AddObject(module, "Notification", (void *)&NotificationType) != 0) {
        Py_DECREF(&NotificationType);
        goto error;
    }

    Py_XINCREF(NCollectdError);
    if (PyModule_AddObject(module, "NCollectdError", NCollectdError) != 0) {
        Py_XDECREF(NCollectdError);
        goto error;
    }

    PyModule_AddIntConstant(module, "LOG_DEBUG", LOG_DEBUG);
    PyModule_AddIntConstant(module, "LOG_INFO", LOG_INFO);
    PyModule_AddIntConstant(module, "LOG_NOTICE", LOG_NOTICE);
    PyModule_AddIntConstant(module, "LOG_WARNING", LOG_WARNING);
    PyModule_AddIntConstant(module, "LOG_ERROR", LOG_ERR);
    PyModule_AddIntConstant(module, "NOTIF_FAILURE", NOTIF_FAILURE);
    PyModule_AddIntConstant(module, "NOTIF_WARNING", NOTIF_WARNING);
    PyModule_AddIntConstant(module, "NOTIF_OKAY", NOTIF_OKAY);
    PyModule_AddIntConstant(module, "METRIC_UNKNOWN", METRIC_TYPE_UNKNOWN);
    PyModule_AddIntConstant(module, "METRIC_GAUGE", METRIC_TYPE_GAUGE);
    PyModule_AddIntConstant(module, "METRIC_COUNTER", METRIC_TYPE_COUNTER);
    PyModule_AddIntConstant(module, "METRIC_STATE_SET", METRIC_TYPE_STATE_SET);
    PyModule_AddIntConstant(module, "METRIC_INFO", METRIC_TYPE_INFO);
    PyModule_AddIntConstant(module, "METRIC_SUMMARY", METRIC_TYPE_SUMMARY);
    PyModule_AddIntConstant(module, "METRIC_HISTOGRAM", METRIC_TYPE_HISTOGRAM);
    PyModule_AddIntConstant(module, "METRIC_GAUGE_HISTOGRAM", METRIC_TYPE_GAUGE_HISTOGRAM);

    return 0;

error:
    cpy_log_exception("exporting Python types");
#ifdef IS_PY3K
    Py_DECREF(module);
#endif
    return 1;
}

static int cpy_config_module(config_item_t *ci)
{
    char *name = NULL;
    int status = cf_util_get_string(ci, &name);
    if (status != 0)
        return -1;

    cpy_callback_t *c;
    for (c = cpy_config_callbacks; c; c = c->next) {
        if (strcasecmp(c->name, name) == 0)
            break;
    }

    if (c == NULL) {
        PLUGIN_WARNING("Found a configuration for the '%s' plugin, "
                       "but the plugin isn't loaded or didn't register "
                       "a configuration callback.", name);
        free(name);
        return 0;
    }

    free(name);

    PyObject *ret;
    if (c->data == NULL)
        ret = PyObject_CallFunction(c->callback, "N", cpy_config_to_pyconfig(ci, NULL)); /* New reference. */
    else
        ret = PyObject_CallFunction(c->callback, "NO", cpy_config_to_pyconfig(ci, NULL),
                                                 c->data); /* New reference. */
    if (ret == NULL) {
        cpy_log_exception("loading module");
        return -1;
    } else {
        Py_DECREF(ret);
    }

    return 0;
}

static int cpy_config(config_item_t *ci)
{
    PyObject *tb;
    int status = 0;

    /* Ok in theory we shouldn't do initialization at this point
     * but we have to. In order to give python scripts a chance
     * to register a config callback we need to be able to execute
     * python code during the config callback so we have to start
     * the interpreter here. */
    /* Do *not* use the python "thread" module at this point! */

    if (!Py_IsInitialized() && cpy_init_python())
        return 1;

    for (int i = 0; i < ci->children_num; ++i) {
        config_item_t *item = ci->children + i;

        if (strcasecmp(item->key, "interactive") == 0) {
            status = cf_util_get_boolean(item, &do_interactive);
        } else if (strcasecmp(item->key, "encoding") == 0) {
            char *encoding = NULL;
            status = cf_util_get_string(item, &encoding);
#ifdef IS_PY3K
            if (status == 0) {
                PLUGIN_ERROR("'encoding' was used in the config file but Python3 was "
                              "used, which does not support changing encodings");
                status = 1;
            }
#else
            /* Why is this even necessary? And undocumented? */
            if (PyUnicode_SetDefaultEncoding(encoding)) {
                cpy_log_exception("setting default encoding");
                status = 1;
            }
#endif
            free(encoding);
        } else if (strcasecmp(item->key, "log-traces") == 0) {
            bool log_traces;
            status = cf_util_get_boolean(item, &log_traces);
            if (status == 0) {
                if (!log_traces) {
                    Py_XDECREF(cpy_format_exception);
                    cpy_format_exception = NULL;
                    continue;
                }

                if (cpy_format_exception)
                    continue;

                tb = PyImport_ImportModule("traceback"); /* New reference. */
                if (tb == NULL) {
                    cpy_log_exception("python initialization");
                    status = 1;
                    break;
                }
                cpy_format_exception = PyObject_GetAttrString(tb, "format_exception"); /* New reference. */
                Py_DECREF(tb);
                if (cpy_format_exception == NULL) {
                    cpy_log_exception("python initialization");
                    status = 1;
                    break;
                }
            }
        } else if (strcasecmp(item->key, "module-path") == 0) {
            char *dir = NULL;
            PyObject *dir_object;

            if (cf_util_get_string(item, &dir) != 0) {
                status = 1;
                break;
            }
            dir_object = cpy_string_to_unicode_or_bytes(dir); /* New reference. */
            if (dir_object == NULL) {
                ERROR("python plugin: Unable to convert \"%s\" to a python object.", dir);
                free(dir);
                cpy_log_exception("python initialization");
                status = 1;
                break;
            }
            if (PyList_Insert(sys_path, 0, dir_object) != 0) {
                ERROR("python plugin: Unable to prepend \"%s\" to "
                      "python module path.", dir);
                cpy_log_exception("python initialization");
                status = 1;
                break;
            }
            Py_DECREF(dir_object);
            free(dir);
        } else if (strcasecmp(item->key, "load-plugin") == 0) {
            char *module_name = NULL;
            PyObject *module;

            if (cf_util_get_string(item, &module_name) != 0) {
                status = 1;
                break;
            }
            module = PyImport_ImportModule(module_name); /* New reference. */
            if (module == NULL) {
                PLUGIN_ERROR("Error importing module \"%s\".", module_name);
                cpy_log_exception("importing module");
                status = 1;
                break;
            }
            free(module_name);
            Py_XDECREF(module);
        } else if (strcasecmp(item->key, "plugin") == 0) {
            status = cpy_config_module(item);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         item->key, cf_get_file(item), cf_get_lineno(item));
            status = 1;
        }

        if (status != 0)
            break;
    }

    return status;
}

void module_register(void)
{
    plugin_register_config("python", cpy_config);
    plugin_register_init("python", cpy_init);
    plugin_register_shutdown("python", cpy_shutdown);
}
