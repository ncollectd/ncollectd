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

#if 0
typedef struct {
    int (*add_string)(void *, const char *, const char *);
    int (*add_signed_int)(void *, const char *, int64_t);
    int (*add_unsigned_int)(void *, const char *, uint64_t);
    int (*add_double)(void *, const char *, double);
    int (*add_boolean)(void *, const char *, bool);
} cpy_build_meta_handler_t;

#define FreeAll()                      \
    do {                               \
        PyMem_Free(type);              \
        PyMem_Free(plugin_instance);   \
        PyMem_Free(type_instance);     \
        PyMem_Free(plugin);            \
        PyMem_Free(host);              \
    } while (0)

static PyObject *cpy_common_repr(PyObject *s)
{
    PyObject *ret, *tmp;
    static PyObject *l_type, *l_type_instance, *l_plugin, *l_plugin_instance;
    static PyObject *l_host, *l_time;
    PluginData *self = (PluginData *)s;

    if (l_type == NULL)
        l_type = cpy_string_to_unicode_or_bytes("(type=");
    if (l_type_instance == NULL)
        l_type_instance = cpy_string_to_unicode_or_bytes(",type_instance=");
    if (l_plugin == NULL)
        l_plugin = cpy_string_to_unicode_or_bytes(",plugin=");
    if (l_plugin_instance == NULL)
        l_plugin_instance = cpy_string_to_unicode_or_bytes(",plugin_instance=");
    if (l_host == NULL)
        l_host = cpy_string_to_unicode_or_bytes(",host=");
    if (l_time == NULL)
        l_time = cpy_string_to_unicode_or_bytes(",time=");

    if (!l_type || !l_type_instance || !l_plugin || !l_plugin_instance ||
            !l_host || !l_time)
        return NULL;

    ret = cpy_string_to_unicode_or_bytes(s->ob_type->tp_name);

    CPY_STRCAT(&ret, l_type);
    tmp = cpy_string_to_unicode_or_bytes(self->type);
    CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
    CPY_STRCAT_AND_DEL(&ret, tmp);

    if (self->type_instance[0] != 0) {
        CPY_STRCAT(&ret, l_type_instance);
        tmp = cpy_string_to_unicode_or_bytes(self->type_instance);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->plugin[0] != 0) {
        CPY_STRCAT(&ret, l_plugin);
        tmp = cpy_string_to_unicode_or_bytes(self->plugin);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->plugin_instance[0] != 0) {
        CPY_STRCAT(&ret, l_plugin_instance);
        tmp = cpy_string_to_unicode_or_bytes(self->plugin_instance);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->host[0] != 0) {
        CPY_STRCAT(&ret, l_host);
        tmp = cpy_string_to_unicode_or_bytes(self->host);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }

    if (self->time != 0) {
        CPY_STRCAT(&ret, l_time);
        tmp = PyFloat_FromDouble(self->time);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }
    return ret;
}

static char time_doc[] =
        "This is the Unix timestamp of the time this value was read.\n"
        "For dispatching values this can be set to 0 which means \"now\".\n"
        "This means the time the value is actually dispatched, not the time\n"
        "it was set to 0.";

static char host_doc[] =
        "The hostname of the host this value was read from.\n"
        "For dispatching this can be set to an empty string which means\n"
        "the local hostname as defined in ncollectd.conf.";

static char type_doc[] =
        "The type of this value. This type has to be defined\n"
        "in the types.db file. Attempting to set it to any other value\n"
        "will raise a TypeError exception.\n"
        "Assigning a type is mandatory, calling dispatch without doing\n"
        "so will raise a RuntimeError exception.";

static char type_instance_doc[] = "";

static char plugin_doc[] =
        "The name of the plugin that read the data. Setting this\n"
        "member to an empty string will insert \"python\" upon dispatching.";

static char plugin_instance_doc[] = "";

static char PluginData_doc[] =
        "This is an internal class that is the base for Values\n"
        "and Notification. It is pretty useless by itself and is therefore not\n"
        "exported to the ncollectd module.";

static PyObject *PluginData_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PluginData *self;

    self = (PluginData *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->time = 0;
    self->host[0] = 0;
    self->plugin[0] = 0;
    self->plugin_instance[0] = 0;
    self->type[0] = 0;
    self->type_instance[0] = 0;
    return (PyObject *)self;
}

static int PluginData_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    PluginData *self = (PluginData *)s;
    double time = 0;
    char *type = NULL, *plugin_instance = NULL, *type_instance = NULL,
             *plugin = NULL, *host = NULL;
    static char *kwlist[] = {
            "type", "plugin_instance", "type_instance", "plugin", "host", "time",
            NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|etetetetetd", kwlist, NULL,
                                           &type, NULL, &plugin_instance, NULL,
                                           &type_instance, NULL, &plugin, NULL, &host,
                                           &time))
        return -1;

    if (type && plugin_get_ds(type) == NULL) {
        PyErr_Format(PyExc_TypeError, "Dataset %s not found", type);
        FreeAll();
        return -1;
    }

    sstrncpy(self->host, host ? host : "", sizeof(self->host));
    sstrncpy(self->plugin, plugin ? plugin : "", sizeof(self->plugin));
    sstrncpy(self->plugin_instance, plugin_instance ? plugin_instance : "",
                     sizeof(self->plugin_instance));
    sstrncpy(self->type, type ? type : "", sizeof(self->type));
    sstrncpy(self->type_instance, type_instance ? type_instance : "",
                     sizeof(self->type_instance));
    self->time = time;

    FreeAll();

    return 0;
}

static PyObject *PluginData_repr(PyObject *s)
{
    PyObject *ret;
    static PyObject *l_closing;

    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if (l_closing == NULL)
        return NULL;

    ret = cpy_common_repr(s);
    CPY_STRCAT(&ret, l_closing);
    return ret;
}

static PyMemberDef PluginData_members[] ={
        {"time", T_DOUBLE, offsetof(PluginData, time), 0, time_doc}, {NULL}
};

static PyObject *PluginData_getstring(PyObject *self, void *data)
{
    const char *value = ((char *)self) + (intptr_t)data;

    return cpy_string_to_unicode_or_bytes(value);
}

static int PluginData_setstring(PyObject *self, PyObject *value, void *data)
{
    char *old;
    const char *new;

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute");
        return -1;
    }
    Py_INCREF(value);
    new = cpy_unicode_or_bytes_to_string(&value);
    if (new == NULL) {
        Py_DECREF(value);
        return -1;
    }
    old = ((char *)self) + (intptr_t)data;
    sstrncpy(old, new, DATA_MAX_NAME_LEN);
    Py_DECREF(value);
    return 0;
}

static int PluginData_settype(PyObject *self, PyObject *value, void *data)
{
    char *old;
    const char *new;

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete this attribute");
        return -1;
    }
    Py_INCREF(value);
    new = cpy_unicode_or_bytes_to_string(&value);
    if (new == NULL) {
        Py_DECREF(value);
        return -1;
    }

    if (plugin_get_ds(new) == NULL) {
        PyErr_Format(PyExc_TypeError, "Dataset %s not found", new);
        Py_DECREF(value);
        return -1;
    }

    old = ((char *)self) + (intptr_t)data;
    sstrncpy(old, new, DATA_MAX_NAME_LEN);
    Py_DECREF(value);
    return 0;
}

static PyGetSetDef PluginData_getseters[] = {
    {"host", PluginData_getstring, PluginData_setstring, host_doc, (void *)offsetof(PluginData, host)},
    {"plugin", PluginData_getstring, PluginData_setstring, plugin_doc, (void *)offsetof(PluginData, plugin)},
    {"plugin_instance", PluginData_getstring, PluginData_setstring, plugin_instance_doc, (void *)offsetof(PluginData, plugin_instance)},
    {"type_instance", PluginData_getstring, PluginData_setstring, type_instance_doc, (void *)offsetof(PluginData, type_instance)},
    {"type", PluginData_getstring, PluginData_settype, type_doc, (void *)offsetof(PluginData, type)},
    {NULL}
};

PyTypeObject PluginDataType = {
    CPY_INIT_TYPE "ncollectd.PluginData", /* tp_name */
    sizeof(PluginData),                  /* tp_basicsize */
    0,                                   /* Will be filled in later */
    0,                                   /* tp_dealloc */
    0,                                   /* tp_print */
    0,                                   /* tp_getattr */
    0,                                   /* tp_setattr */
    0,                                   /* tp_compare */
    PluginData_repr,                     /* tp_repr */
    0,                                   /* tp_as_number */
    0,                                   /* tp_as_sequence */
    0,                                   /* tp_as_mapping */
    0,                                   /* tp_hash */
    0,                                   /* tp_call */
    0,                                   /* tp_str */
    0,                                   /* tp_getattro */
    0,                                   /* tp_setattro */
    0,                                   /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT Py_TPFLAGS_BASETYPE /*| Py_TPFLAGS_HAVE_GC*/, /*tp_flags*/
    PluginData_doc,                      /* tp_doc */
    0,                                   /* tp_traverse */
    0,                                   /* tp_clear */
    0,                                   /* tp_richcompare */
    0,                                   /* tp_weaklistoffset */
    0,                                   /* tp_iter */
    0,                                   /* tp_iternext */
    0,                                   /* tp_methods */
    PluginData_members,                  /* tp_members */
    PluginData_getseters,                /* tp_getset */
    0,                                   /* tp_base */
    0,                                   /* tp_dict */
    0,                                   /* tp_descr_get */
    0,                                   /* tp_descr_set */
    0,                                   /* tp_dictoffset */
    PluginData_init,                     /* tp_init */
    0,                                   /* tp_alloc */
    PluginData_new                       /* tp_new */
};

static char interval_doc[] =
    "The interval is the timespan in seconds between two submits for\n"
    "the same data source. This value has to be a positive integer, so you can't\n"
    "submit more than one value per second. If this member is set to a\n"
    "non-positive value, the default value as specified in the config file will\n"
    "be used (default: 10).\n"
    "\n"
    "If you submit values more often than the specified interval, the average\n"
    "will be used. If you submit less values, your graphs will have gaps.";

static char values_doc[] =
    "These are the actual values that get dispatched to ncollectd.\n"
    "It has to be a sequence (a tuple or list) of numbers.\n"
    "The size of the sequence and the type of its content depend on the type\n"
    "member in the types.db file. For more information on this read the\n"
    "types.db man page.\n"
    "\n"
    "If the sequence does not have the correct size upon dispatch a RuntimeError\n"
    "exception will be raised. If the content of the sequence is not a number,\n"
    "a TypeError exception will be raised.";

static char meta_doc[] =
    "These are the meta data for this Value object.\n"
    "It has to be a dictionary of numbers, strings or bools. All keys must be\n"
    "strings. int and long objects will be dispatched as signed integers unless\n"
    "they are between 2**63 and 2**64-1, which will result in an unsigned integer.\n"
    "You can force one of these storage classes by using the classes\n"
    "ncollectd.Signed and ncollectd.Unsigned. A meta object received by a write\n"
    "callback will always contain Signed or Unsigned objects.";

static char dispatch_doc[] =
    "dispatch([type][, values][, plugin_instance][, type_instance]"
    "[, plugin][, host][, time][, interval]) -> None.  Dispatch a value list.\n"
    "\n"
    "Dispatch this instance to the ncollectd process. The object has members\n"
    "for each of the possible arguments for this method. For a detailed explanation\n"
    "of these parameters see the member of the same same.\n"
    "\n"
    "If you do not submit a parameter the value saved in its member will be submitted.\n"
    "If you do provide a parameter it will be used instead, without altering the member.";

static char write_doc[] =
    "write([destination][, type][, values][, plugin_instance][, type_instance]"
    "[, plugin][, host][, time][, interval]) -> None.  Dispatch a value list.\n"
    "\n"
    "Write this instance to a single plugin or all plugins if 'destination' is omitted.\n"
    "This will bypass the main ncollectd process and all filtering and caching.\n"
    "Other than that it works similar to 'dispatch'. In most cases 'dispatch' should be\n"
    "used instead of 'write'.\n";

static char Values_doc[] = "A Values object used for dispatching values to "
                           "ncollectd and receiving values from write callbacks.";

static PyObject *Values_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Values *self;

    self = (Values *)PluginData_new(type, args, kwds);
    if (self == NULL)
        return NULL;

    self->values = PyList_New(0);
    self->meta = PyDict_New();
    self->interval = 0;
    return (PyObject *)self;
}

static int Values_init(PyObject *s, PyObject *args, PyObject *kwds)
{
    Values *self = (Values *)s;
    double interval = 0, time = 0;
    PyObject *values = NULL, *meta = NULL, *tmp;
    char *type = NULL, *plugin_instance = NULL, *type_instance = NULL,
             *plugin = NULL, *host = NULL;
    static char *kwlist[] = {
            "type", "values", "plugin_instance", "type_instance", "plugin",
            "host", "time",     "interval",      "meta",          NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|etOetetetetddO", kwlist, NULL,
                                           &type, &values, NULL, &plugin_instance, NULL,
                                           &type_instance, NULL, &plugin, NULL, &host,
                                           &time, &interval, &meta))
        return -1;

    if (type && plugin_get_ds(type) == NULL) {
        PyErr_Format(PyExc_TypeError, "Dataset %s not found", type);
        FreeAll();
        return -1;
    }

    sstrncpy(self->data.host, host ? host : "", sizeof(self->data.host));
    sstrncpy(self->data.plugin, plugin ? plugin : "", sizeof(self->data.plugin));
    sstrncpy(self->data.plugin_instance, plugin_instance ? plugin_instance : "",
                     sizeof(self->data.plugin_instance));
    sstrncpy(self->data.type, type ? type : "", sizeof(self->data.type));
    sstrncpy(self->data.type_instance, type_instance ? type_instance : "",
                     sizeof(self->data.type_instance));
    self->data.time = time;

    FreeAll();

    if (values == NULL) {
        values = PyList_New(0);
        PyErr_Clear();
    } else {
        Py_INCREF(values);
    }

    if (meta == NULL) {
        meta = PyDict_New();
        PyErr_Clear();
    } else {
        Py_INCREF(meta);
    }

    tmp = self->values;
    self->values = values;
    Py_XDECREF(tmp);

    tmp = self->meta;
    self->meta = meta;
    Py_XDECREF(tmp);

    self->interval = interval;
    return 0;
}

static int cpy_build_meta_generic(PyObject *meta, cpy_build_meta_handler_t *meta_func, void *m)
{
    int s;
    PyObject *l;

    if ((meta == NULL) || (meta == Py_None))
        return -1;

    l = PyDict_Items(meta); /* New reference. */
    if (!l) {
        cpy_log_exception("building meta data");
        return -1;
    }
    s = PyList_Size(l);
    if (s <= 0) {
        Py_XDECREF(l);
        return -1;
    }

    for (int i = 0; i < s; ++i) {
        const char *string, *keystring;
        PyObject *key, *value, *item, *tmp;

        item = PyList_GET_ITEM(l, i);
        key = PyTuple_GET_ITEM(item, 0);
        Py_INCREF(key);
        keystring = cpy_unicode_or_bytes_to_string(&key);
        if (!keystring) {
            PyErr_Clear();
            Py_XDECREF(key);
            continue;
        }
        value = PyTuple_GET_ITEM(item, 1);
        Py_INCREF(value);
        if (value == Py_True) {
            meta_func->add_boolean(m, keystring, 1);
        } else if (value == Py_False) {
            meta_func->add_boolean(m, keystring, 0);
        } else if (PyFloat_Check(value)) {
            meta_func->add_double(m, keystring, PyFloat_AsDouble(value));
        } else if (PyObject_TypeCheck(value, &SignedType)) {
            long long int lli;
            lli = PyLong_AsLongLong(value);
            if (!PyErr_Occurred() && (lli == (int64_t)lli))
                meta_func->add_signed_int(m, keystring, lli);
        } else if (PyObject_TypeCheck(value, &UnsignedType)) {
            long long unsigned llu;
            llu = PyLong_AsUnsignedLongLong(value);
            if (!PyErr_Occurred() && (llu == (uint64_t)llu))
                meta_func->add_unsigned_int(m, keystring, llu);
        } else if (PyNumber_Check(value)) {
            long long int lli;
            long long unsigned llu;
            tmp = PyNumber_Long(value);
            lli = PyLong_AsLongLong(tmp);
            if (!PyErr_Occurred() && (lli == (int64_t)lli)) {
                meta_func->add_signed_int(m, keystring, lli);
            } else {
                PyErr_Clear();
                llu = PyLong_AsUnsignedLongLong(tmp);
                if (!PyErr_Occurred() && (llu == (uint64_t)llu))
                    meta_func->add_unsigned_int(m, keystring, llu);
            }
            Py_XDECREF(tmp);
        } else {
            string = cpy_unicode_or_bytes_to_string(&value);
            if (string) {
                meta_func->add_string(m, keystring, string);
            } else {
                PyErr_Clear();
                tmp = PyObject_Str(value);
                string = cpy_unicode_or_bytes_to_string(&tmp);
                if (string)
                    meta_func->add_string(m, keystring, string);
                Py_XDECREF(tmp);
            }
        }
        if (PyErr_Occurred())
            cpy_log_exception("building meta data");
        Py_XDECREF(value);
        Py_DECREF(key);
    }
    Py_XDECREF(l);
    return 0;
}

#define CPY_BUILD_META_FUNC(meta_type, func, val_type)                   \
    static int cpy_##func(void *meta, const char *key, val_type val) {   \
        return func((meta_type *)meta, key, val);                        \
    }

#define CPY_BUILD_META_HANDLER(func_prefix, meta_type)                       \
    CPY_BUILD_META_FUNC(meta_type, func_prefix##_add_string, const char *)   \
    CPY_BUILD_META_FUNC(meta_type, func_prefix##_add_signed_int, int64_t)    \
    CPY_BUILD_META_FUNC(meta_type, func_prefix##_add_unsigned_int, uint64_t) \
    CPY_BUILD_META_FUNC(meta_type, func_prefix##_add_double, double)         \
    CPY_BUILD_META_FUNC(meta_type, func_prefix##_add_boolean, bool)          \
                                                                             \
    static cpy_build_meta_handler_t cpy_##func_prefix = {                    \
            .add_string = cpy_##func_prefix##_add_string,                    \
            .add_signed_int = cpy_##func_prefix##_add_signed_int,            \
            .add_unsigned_int = cpy_##func_prefix##_add_unsigned_int,        \
            .add_double = cpy_##func_prefix##_add_double,                    \
            .add_boolean = cpy_##func_prefix##_add_boolean}

CPY_BUILD_META_HANDLER(meta_data, meta_data_t);
CPY_BUILD_META_HANDLER(plugin_notification_meta, notification_t);

static meta_data_t *cpy_build_meta(PyObject *meta)
{
    meta_data_t *m = meta_data_create();
    if (cpy_build_meta_generic(meta, &cpy_meta_data, (void *)m) < 0) {
        meta_data_destroy(m);
        return NULL;
    }
    return m;
}

static void cpy_build_notification_meta(notification_t *n, PyObject *meta)
{
    cpy_build_meta_generic(meta, &cpy_plugin_notification_meta, (void *)n);
}

static PyObject *Values_dispatch(Values *self, PyObject *args, PyObject *kwds)
{
    int ret;
    const data_set_t *ds;
    size_t size;
    value_t *value;
    value_list_t value_list = VALUE_LIST_INIT;
    PyObject *values = self->values, *meta = self->meta;
    double time = self->data.time, interval = self->interval;
    char *host = NULL, *plugin = NULL, *plugin_instance = NULL, *type = NULL,
             *type_instance = NULL;

    static char *kwlist[] = {
            "type", "values", "plugin_instance", "type_instance", "plugin",
            "host", "time",     "interval",              "meta",                    NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|etOetetetetddO", kwlist, NULL,
                                           &type, &values, NULL, &plugin_instance, NULL,
                                           &type_instance, NULL, &plugin, NULL, &host,
                                           &time, &interval, &meta))
        return NULL;

    sstrncpy(value_list.host, host ? host : self->data.host,
                     sizeof(value_list.host));
    sstrncpy(value_list.plugin, plugin ? plugin : self->data.plugin,
                     sizeof(value_list.plugin));
    sstrncpy(value_list.plugin_instance,
                     plugin_instance ? plugin_instance : self->data.plugin_instance,
                     sizeof(value_list.plugin_instance));
    sstrncpy(value_list.type, type ? type : self->data.type,
                     sizeof(value_list.type));
    sstrncpy(value_list.type_instance,
                     type_instance ? type_instance : self->data.type_instance,
                     sizeof(value_list.type_instance));
    FreeAll();
    if (value_list.type[0] == 0) {
        PyErr_SetString(PyExc_RuntimeError, "type not set");
        FreeAll();
        return NULL;
    }
    ds = plugin_get_ds(value_list.type);
    if (ds == NULL) {
        PyErr_Format(PyExc_TypeError, "Dataset %s not found", value_list.type);
        return NULL;
    }
    if (values == NULL ||
            (PyTuple_Check(values) == 0 && PyList_Check(values) == 0)) {
        PyErr_Format(PyExc_TypeError, "values must be list or tuple");
        return NULL;
    }
    if (meta != NULL && meta != Py_None && !PyDict_Check(meta)) {
        PyErr_Format(PyExc_TypeError, "meta must be a dict");
        return NULL;
    }
    size = (size_t)PySequence_Length(values);
    if (size != ds->ds_num) {
        PyErr_Format(PyExc_RuntimeError,
                     "type %s needs %" PRIsz " values, got %" PRIsz,
                     value_list.type, ds->ds_num, size);
        return NULL;
    }
    value = calloc(size, sizeof(*value));
    for (size_t i = 0; i < size; ++i) {
        PyObject *item, *num;
        item = PySequence_Fast_GET_ITEM(values, (int)i); /* Borrowed reference. */
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
        if (PyErr_Occurred() != NULL) {
            free(value);
            return NULL;
        }
    }
    value_list.values = value;
    value_list.meta = cpy_build_meta(meta);
    value_list.values_len = size;
    value_list.time = DOUBLE_TO_CDTIME_T(time);
    value_list.interval = DOUBLE_TO_CDTIME_T(interval);
    if (value_list.host[0] == 0)
        sstrncpy(value_list.host, hostname_g, sizeof(value_list.host));
    if (value_list.plugin[0] == 0)
        sstrncpy(value_list.plugin, "python", sizeof(value_list.plugin));
    Py_BEGIN_ALLOW_THREADS;
    ret = plugin_dispatch_values(&value_list);
    Py_END_ALLOW_THREADS;
    meta_data_destroy(value_list.meta);
    free(value);
    if (ret != 0) {
        PyErr_SetString(PyExc_RuntimeError, "error dispatching values, read the logs");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Values_write(Values *self, PyObject *args, PyObject *kwds)
{
    int ret;
    const data_set_t *ds;
    size_t size;
    metric_single_t metric = STRUCT_METRIC_INIT;
    PyObject *values = self->values, *meta = self->meta;
    double time = self->data.time, interval = self->interval;
    char *host = NULL, *plugin = NULL, *type = NULL, *data_source = NULL,
             *dest = NULL;

    static char *kwlist[] = {"destination", "type", "values", "dat_source",
                             "plugin",          "host", "time",     "interval",
                             "meta",                NULL};
    if (!PyArg_ParseTupleAndKeywords(
                    args, kwds, "et|etOetetetetdiO", kwlist, NULL, &dest, NULL, &type,
                    &values, NULL, &plugin_instance, NULL, &type_instance, NULL, &plugin,
                    NULL, &host, &time, &interval, &meta))
        return NULL;

    metric.identity = identity_create_legacy(
            (plugin ? plugin : self->data.plugin), (type ? type : self->data.type),
            (host ? host : self->data.host));

    sstrncpy(value_list.host, host ? host : self->data.host,
                     sizeof(value_list.host));
    sstrncpy(value_list.plugin, plugin ? plugin : self->data.plugin,
                     sizeof(value_list.plugin));
    sstrncpy(value_list.plugin_instance,
                     plugin_instance ? plugin_instance : self->data.plugin_instance,
                     sizeof(value_list.plugin_instance));
    sstrncpy(value_list.type, type ? type : self->data.type,
                     sizeof(value_list.type));
    sstrncpy(value_list.type_instance,
                     type_instance ? type_instance : self->data.type_instance,
                     sizeof(value_list.type_instance));
    FreeAll();
    if (value_list.type[0] == 0) {
        PyErr_SetString(PyExc_RuntimeError, "type not set");
        return NULL;
    }
    ds = plugin_get_ds((type ? type : self->data.type));
    if (ds == NULL) {
        PyErr_Format(PyExc_TypeError, "Dataset %s not found", value_list.type);
        return NULL;
    }
    if (values == NULL ||
            (PyTuple_Check(values) == 0 && PyList_Check(values) == 0)) {
        PyErr_Format(PyExc_TypeError, "values must be list or tuple");
        return NULL;
    }
    size = (size_t)PySequence_Length(values);
    if (size != ds->ds_num) {
        PyErr_Format(PyExc_RuntimeError,
                                 "type %s needs %" PRIsz " values, got %" PRIsz,
                                 value_list.type, ds->ds_num, size);
        return NULL;
    }
    value = calloc(size, sizeof(*value));
    for (size_t i = 0; i < size; ++i) {
        PyObject *item, *num;
        item = PySequence_Fast_GET_ITEM(values, i); /* Borrowed reference. */
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
            PyErr_Format(PyExc_RuntimeError, "unknown data type %d for %s",
                                     ds->ds[i].type, value_list.type);
            return NULL;
        }
        if (PyErr_Occurred() != NULL) {
            free(value);
            return NULL;
        }
    }
    value_list.values = value;
    value_list.values_len = size;
    value_list.time = DOUBLE_TO_CDTIME_T(time);
    value_list.interval = DOUBLE_TO_CDTIME_T(interval);
    value_list.meta = cpy_build_meta(meta);
    if (value_list.host[0] == 0)
        sstrncpy(value_list.host, hostname_g, sizeof(value_list.host));
    if (value_list.plugin[0] == 0)
        sstrncpy(value_list.plugin, "python", sizeof(value_list.plugin));
    Py_BEGIN_ALLOW_THREADS;
    ret = plugin_write(dest, NULL, &value_list);
    Py_END_ALLOW_THREADS;
    meta_data_destroy(value_list.meta);
    free(value);
    if (ret != 0) {
        PyErr_SetString(PyExc_RuntimeError,
                                        "error dispatching values, read the logs");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Values_repr(PyObject *s)
{
    PyObject *ret, *tmp;
    static PyObject *l_interval, *l_values, *l_meta, *l_closing;
    Values *self = (Values *)s;

    if (l_interval == NULL)
        l_interval = cpy_string_to_unicode_or_bytes(",interval=");
    if (l_values == NULL)
        l_values = cpy_string_to_unicode_or_bytes(",values=");
    if (l_meta == NULL)
        l_meta = cpy_string_to_unicode_or_bytes(",meta=");
    if (l_closing == NULL)
        l_closing = cpy_string_to_unicode_or_bytes(")");

    if (l_interval == NULL || l_values == NULL || l_meta == NULL ||
            l_closing == NULL)
        return NULL;

    ret = cpy_common_repr(s);
    if (self->interval != 0) {
        CPY_STRCAT(&ret, l_interval);
        tmp = PyFloat_FromDouble(self->interval);
        CPY_SUBSTITUTE(PyObject_Repr, tmp, tmp);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }
    if (self->values &&
            (!PyList_Check(self->values) || PySequence_Length(self->values) > 0)) {
        CPY_STRCAT(&ret, l_values);
        tmp = PyObject_Repr(self->values);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }
    if (self->meta &&
            (!PyDict_Check(self->meta) || PyDict_Size(self->meta) > 0)) {
        CPY_STRCAT(&ret, l_meta);
        tmp = PyObject_Repr(self->meta);
        CPY_STRCAT_AND_DEL(&ret, tmp);
    }
    CPY_STRCAT(&ret, l_closing);
    return ret;
}

static int Values_traverse(PyObject *self, visitproc visit, void *arg)
{
    Values *v = (Values *)self;
    Py_VISIT(v->values);
    Py_VISIT(v->meta);
    return 0;
}

static int Values_clear(PyObject *self)
{
    Values *v = (Values *)self;
    Py_CLEAR(v->values);
    Py_CLEAR(v->meta);
    return 0;
}

static void Values_dealloc(PyObject *self)
{
    Values_clear(self);
    self->ob_type->tp_free(self);
}

static PyMemberDef Values_members[] = {
        {"interval", T_DOUBLE, offsetof(Values, interval), 0, interval_doc},
        {"values", T_OBJECT_EX, offsetof(Values, values), 0, values_doc},
        {"meta", T_OBJECT_EX, offsetof(Values, meta), 0, meta_doc},
        {NULL}
};

static PyMethodDef Values_methods[] = {
        {"dispatch", (PyCFunction)Values_dispatch, METH_VARARGS | METH_KEYWORDS, dispatch_doc},
        {"write", (PyCFunction)Values_write, METH_VARARGS | METH_KEYWORDS, write_doc},
        {NULL}
};

PyTypeObject ValuesType = {
    CPY_INIT_TYPE "ncollectd.Values", /* tp_name */
    sizeof(Values),                  /* tp_basicsize */
    0,                               /* Will be filled in later */
    Values_dealloc,                  /* tp_dealloc */
    0,                               /* tp_print */
    0,                               /* tp_getattr */
    0,                               /* tp_setattr */
    0,                               /* tp_compare */
    Values_repr,                     /* tp_repr */
    0,                               /* tp_as_number */
    0,                               /* tp_as_sequence */
    0,                               /* tp_as_mapping */
    0,                               /* tp_hash */
    0,                               /* tp_call */
    0,                               /* tp_str */
    0,                               /* tp_getattro */
    0,                               /* tp_setattro */
    0,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    Values_doc,                      /* tp_doc */
    Values_traverse,                 /* tp_traverse */
    Values_clear,                    /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    Values_methods,                  /* tp_methods */
    Values_members,                  /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    Values_init,                     /* tp_init */
    0,                               /* tp_alloc */
    Values_new                       /* tp_new */
};
#endif


static char Metric_time_doc[] =
    "The name of the metric family.";

static char Metric_interval_doc[] =
    "The name of the metric family.";

static char Metric_labels_doc[] = "";

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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
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
    Metric *n = (Metric *)self;
    Py_VISIT(n->labels);
    return 0;
}

static int Metric_clear(PyObject *self)
{
    Metric *n = (Metric *)self;
    Py_CLEAR(n->labels);
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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_value = cpy_string_to_unicode_or_bytes("value=");
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
    "The unsigned int value for metric type unknown.";

static char MetricUnknownLong_doc[] =
    "The MetricUnknownLong class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_value = cpy_string_to_unicode_or_bytes("value=");
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
    "The double value for metric type unknown.";

static char MetricGaugeDouble_doc[] =
    "The MetricGaugeDouble class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_value = cpy_string_to_unicode_or_bytes("value=");
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
    "The unsigned int value for metric type unknown.";

static char MetricGaugeLong_doc[] =
    "The MetricGaugeLong class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_value = cpy_string_to_unicode_or_bytes("value=");
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
    "The double value for metric type unknown.";

static char MetricCounterDouble_doc[] =
    "The MetricCounterDouble class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_value = cpy_string_to_unicode_or_bytes("value=");
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
    "The unsigned int value for metric type unknown.";

static char MetricCounterULong_doc[] =
    "The MetricCounterULong class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a dict");
        Py_XDECREF(labels);
        return -1;
    }

    self->value = value;
    self->metric.time = time;
    self->metric.interval = interval;

    if (labels == NULL) {
        labels = PyDict_New();
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_value = cpy_string_to_unicode_or_bytes("value=");
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
    "The unsigned int value for metric type unknown.";

static char MetricInfo_doc[] =
    "The MetricInfo class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((info != NULL) && (!PyDict_Check(info) || PyDict_Size(info) > 0)) {
        PyErr_SetString(PyExc_TypeError, "info must be a dict");
        Py_XDECREF(info);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
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
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_info = cpy_string_to_unicode_or_bytes("info=");
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
    "The unsigned int value for metric type unknown.";

static char MetricStateSet_doc[] =
    "The MetricStateSet class is a wrapper around the ncollectd unknown_t value.";

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
    if (status != 0)
        return -1;

    if ((set != NULL) && (!PyDict_Check(set) || PyDict_Size(set) > 0)) {
        PyErr_SetString(PyExc_TypeError, "set must be a dict");
        Py_XDECREF(set);
        Py_XDECREF(labels);
        return -1;
    }

    if ((labels != NULL) && (!PyDict_Check(labels) || PyDict_Size(labels) > 0)) {
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
        PyErr_Clear();
    }

    if (labels != NULL) {
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
        l_set = cpy_string_to_unicode_or_bytes("set=");
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

