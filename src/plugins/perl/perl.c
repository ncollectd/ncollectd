// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2007-2009 Sebastian Harl
// SPDX-FileContributor: Sebastian Harl <sh at tokkee.org>
// SPDX-FileContributor: Pavel Rochnyak <pavel2000 at ngs.ru>

#include "cperl.h"

#include "libutils/common.h"


/* this is defined in DynaLoader.a */
EXTERN_C void boot_DynaLoader(pTHX_ CV *cv);

extern char **environ;

typedef enum {
    CPL_CALLBACK_CONFIG,
    CPL_CALLBACK_INIT,
    CPL_CALLBACK_READ,
    CPL_CALLBACK_WRITE,
    CPL_CALLBACK_LOG,
    CPL_CALLBACK_NOTIFICATION,
    CPL_CALLBACK_SHUTDOWN,
} cpl_callback_type_t;

typedef struct cpl_callback_s {
    cpl_callback_type_t type;
    char *name;
    SV *callback;
    SV *data;
    /* Set when the callback is unregistered, protected by cpl_lock. */
    bool cancelled;
    struct cpl_callback_s *next;
    /* List of the callbacks freed by the daemon while the interpreter was
     * owned by another thread, protected by cpl_lock. */
    struct cpl_callback_s *zombie_next;
} cpl_callback_t;

typedef struct cpl_log_pending_s {
    struct cpl_log_pending_s *next;
    cpl_callback_t *callback;
    int severity;
    char message[];
} cpl_log_pending_t;

#define CPL_LOG_PENDING_MAX 1024

/* There is only one perl interpreter, and like the python interpreter with
 * its GIL, it can only be used by one thread at a time. The thread that owns
 * the interpreter can enter it again: the perl code calls ncollectd functions
 * that can call perl callbacks in the same thread (i.e. the log callbacks).
 * Unlike the python GIL, the perl interpreter can not be released in the
 * middle of a call because all the threads share the perl stacks, so the
 * callbacks that the daemon can call while it waits for the owner of the
 * interpreter (log callbacks and the free of the user data) never wait for
 * it: the work is queued and done by the owner when it releases the
 * interpreter. */
static pthread_mutex_t cpl_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cpl_cond = PTHREAD_COND_INITIALIZER;
static pthread_t cpl_owner;
static unsigned int cpl_depth;
static PerlInterpreter *cpl_perl;
static cpl_callback_t *cpl_zombies;
static cpl_log_pending_t *cpl_log_pending;
static cpl_log_pending_t **cpl_log_pending_tail = &cpl_log_pending;
static size_t cpl_log_pending_num;
static uint64_t cpl_log_pending_dropped;

/* The following variables are only used by the owner of the interpreter. */
static cpl_callback_t *cpl_config_callbacks;
static cpl_callback_t *cpl_init_callbacks;
static cpl_callback_t *cpl_shutdown_callbacks;
/* read, write, log and notification callbacks registered in the daemon. */
static cpl_callback_t *cpl_daemon_callbacks;
static size_t cpl_num_callbacks;
static bool cpl_shutdown_triggered;
static int cpl_log_depth;
/* An anonymous XSUB that converts a value to a string. */
static CV *cpl_string_cv;

/* Command line options of the perl interpreter. */
static char **cpl_options;
static size_t cpl_options_num;
static char cpl_argv0[] = "";
static char cpl_argv_e[] = "-e";
static char cpl_argv_code[] = "0";
static int cpl_argc;
static char **cpl_argv;
static bool cpl_sys_init;

static char *cpl_base_name;

static void cpl_release(void);
static void cpl_log_deliver(pTHX_ cpl_callback_t *c, int severity, const char *message);

static const char *cpl_callback_type_name(cpl_callback_type_t type)
{
    switch (type) {
    case CPL_CALLBACK_CONFIG:
        return "config";
    case CPL_CALLBACK_INIT:
        return "init";
    case CPL_CALLBACK_READ:
        return "read";
    case CPL_CALLBACK_WRITE:
        return "write";
    case CPL_CALLBACK_LOG:
        return "log";
    case CPL_CALLBACK_NOTIFICATION:
        return "notification";
    case CPL_CALLBACK_SHUTDOWN:
        return "shutdown";
    }
    return "unknown";
}

/* Another thread owns the interpreter, must be called with cpl_lock locked. */
static bool cpl_busy_locked(void)
{
    return (cpl_depth > 0) && !pthread_equal(cpl_owner, pthread_self());
}

/* Takes the interpreter, which is free or owned by this thread, and sets it
 * as the perl context of the thread. Must be called with cpl_lock locked. */
static PerlInterpreter *cpl_take_locked(void)
{
    if (cpl_depth == 0)
        cpl_owner = pthread_self();
    cpl_depth++;

    PERL_SET_CONTEXT(cpl_perl);
    return cpl_perl;
}

/* Waits for the interpreter. Returns NULL if there is no interpreter or if
 * "cancel" is set while waiting: plugin_unregister_write and
 * plugin_unregister_notification wait for the thread that calls the callback,
 * which can be waiting for the interpreter. */
static PerlInterpreter *cpl_acquire(const bool *cancel)
{
    pthread_mutex_lock(&cpl_lock);

    while (cpl_busy_locked() && ((cancel == NULL) || !*cancel))
        pthread_cond_wait(&cpl_cond, &cpl_lock);

    PerlInterpreter *perl = NULL;
    if ((cpl_perl != NULL) && ((cancel == NULL) || !*cancel))
        perl = cpl_take_locked();

    pthread_mutex_unlock(&cpl_lock);

    return perl;
}

static bool cpl_callback_cancelled(cpl_callback_t *c)
{
    pthread_mutex_lock(&cpl_lock);
    bool cancelled = c->cancelled;
    pthread_mutex_unlock(&cpl_lock);
    return cancelled;
}

static void cpl_callback_cancel(cpl_callback_t *c)
{
    pthread_mutex_lock(&cpl_lock);
    c->cancelled = true;
    pthread_cond_broadcast(&cpl_cond);
    pthread_mutex_unlock(&cpl_lock);
}

static cpl_callback_t *cpl_callback_new(pTHX_ cpl_callback_type_t type, const char *name,
                                        CV *code, SV *data)
{
    cpl_callback_t *c = calloc(1, sizeof(*c));
    if (c == NULL)
        croak("calloc failed");

    c->name = strdup(name);
    if (c->name == NULL) {
        free(c);
        croak("strdup failed");
    }

    c->type = type;
    c->callback = newRV_inc((SV *)code);
    c->data = (data != NULL) ? newSVsv(data) : NULL;

    cpl_num_callbacks++;
    return c;
}

static void cpl_callback_free(pTHX_ cpl_callback_t *c)
{
    SvREFCNT_dec(c->callback);
    SvREFCNT_dec(c->data);
    free(c->name);
    free(c);
    cpl_num_callbacks--;
}

static void cpl_callback_list_free(pTHX_ cpl_callback_t **list)
{
    cpl_callback_t *c = *list;
    *list = NULL;

    while (c != NULL) {
        cpl_callback_t *next = c->next;
        cpl_callback_free(aTHX_ c);
        c = next;
    }
}

static bool cpl_daemon_callback_registered(cpl_callback_t *c)
{
    for (cpl_callback_t *cb = cpl_daemon_callbacks; cb != NULL; cb = cb->next) {
        if (cb == c)
            return true;
    }
    return false;
}

static void cpl_daemon_callback_free(pTHX_ cpl_callback_t *c)
{
    for (cpl_callback_t **ptr = &cpl_daemon_callbacks; *ptr != NULL; ptr = &(*ptr)->next) {
        if (*ptr == c) {
            *ptr = c->next;
            break;
        }
    }

    cpl_callback_free(aTHX_ c);
}

/* Must be called with cpl_lock locked. */
static void cpl_log_pending_add_locked(cpl_callback_t *c, int severity, const char *message)
{
    if (cpl_log_pending_num >= CPL_LOG_PENDING_MAX) {
        cpl_log_pending_dropped++;
        return;
    }

    size_t len = strlen(message);
    cpl_log_pending_t *pending = malloc(sizeof(*pending) + len + 1);
    if (pending == NULL) {
        cpl_log_pending_dropped++;
        return;
    }

    pending->next = NULL;
    pending->callback = c;
    pending->severity = severity;
    memcpy(pending->message, message, len + 1);

    *cpl_log_pending_tail = pending;
    cpl_log_pending_tail = &pending->next;
    cpl_log_pending_num++;
}

/* Delivers the log messages and frees the callbacks queued by the other
 * threads while this thread owned the interpreter. */
static void cpl_pending_run(void)
{
    pthread_mutex_lock(&cpl_lock);
    cpl_callback_t *zombies = cpl_zombies;
    cpl_zombies = NULL;
    cpl_log_pending_t *logs = cpl_log_pending;
    cpl_log_pending = NULL;
    cpl_log_pending_tail = &cpl_log_pending;
    cpl_log_pending_num = 0;
    uint64_t dropped = cpl_log_pending_dropped;
    cpl_log_pending_dropped = 0;
    PerlInterpreter *perl = cpl_perl;
    pthread_mutex_unlock(&cpl_lock);

    while (logs != NULL) {
        cpl_log_pending_t *next = logs->next;
        cpl_callback_t *c = logs->callback;
        /* The callback could have been unregistered after the message was queued. */
        if ((perl != NULL) && cpl_daemon_callback_registered(c) && (c->type == CPL_CALLBACK_LOG) &&
            !cpl_callback_cancelled(c)) {
            dTHXa(perl);
            cpl_log_deliver(aTHX_ c, logs->severity, logs->message);
        }
        free(logs);
        logs = next;
    }

    while (zombies != NULL) {
        cpl_callback_t *next = zombies->zombie_next;
        if (perl != NULL) {
            dTHXa(perl);
            cpl_daemon_callback_free(aTHX_ zombies);
        } else {
            free(zombies->name);
            free(zombies);
        }
        zombies = next;
    }

    if (dropped > 0)
        PLUGIN_WARNING("%" PRIu64 " log messages were not sent to the perl log callbacks.",
                       dropped);
}

static void cpl_interpreter_destroy(void)
{
    PerlInterpreter *perl = cpl_perl;
    dTHXa(perl);

    PL_perl_destruct_level = 1;
    perl_destruct(perl);
    perl_free(perl);

    cpl_string_cv = NULL;

    free(cpl_argv);
    cpl_argv = NULL;
    cpl_argc = 0;

    for (size_t i = 0; i < cpl_options_num; i++)
        free(cpl_options[i]);
    free(cpl_options);
    cpl_options = NULL;
    cpl_options_num = 0;
}

/* Like Py_Finalize in the python plugin, the interpreter is destroyed by the
 * last owner after the shutdown, once all the callbacks have been freed. */
static void cpl_release(void)
{
    pthread_mutex_lock(&cpl_lock);

    /* The outermost release runs the work queued by the other threads. */
    for (int i = 0; (i < 16) && (cpl_depth == 1) && ((cpl_zombies != NULL) ||
                    (cpl_log_pending != NULL) || (cpl_log_pending_dropped > 0)); i++) {
        pthread_mutex_unlock(&cpl_lock);
        cpl_pending_run();
        pthread_mutex_lock(&cpl_lock);
    }

    if ((cpl_depth == 1) && cpl_shutdown_triggered && (cpl_num_callbacks == 0) &&
        (cpl_perl != NULL)) {
        pthread_mutex_unlock(&cpl_lock);
        cpl_interpreter_destroy();
        pthread_mutex_lock(&cpl_lock);
        cpl_perl = NULL;
        pthread_mutex_unlock(&cpl_lock);
        /* Free what was queued while the interpreter was destroyed. */
        cpl_pending_run();
        pthread_mutex_lock(&cpl_lock);
    }

    cpl_depth--;
    if (cpl_depth == 0)
        pthread_cond_broadcast(&cpl_cond);

    pthread_mutex_unlock(&cpl_lock);
}

/* The free_func of the callbacks registered in the daemon. */
static void cpl_callback_destroy(void *arg)
{
    cpl_callback_t *c = arg;

    pthread_mutex_lock(&cpl_lock);

    if (cpl_perl == NULL) {
        /* The interpreter has been destroyed with all its values. */
        pthread_mutex_unlock(&cpl_lock);
        free(c->name);
        free(c);
        return;
    }

    if (cpl_busy_locked()) {
        /* The owner of the interpreter can be waiting for this thread:
         * plugin_unregister_write joins the thread that frees the callback. */
        c->cancelled = true;
        c->zombie_next = cpl_zombies;
        cpl_zombies = c;
        pthread_mutex_unlock(&cpl_lock);
        return;
    }

    PerlInterpreter *perl = cpl_take_locked();
    pthread_mutex_unlock(&cpl_lock);
    dTHXa(perl);

    cpl_daemon_callback_free(aTHX_ c);

    cpl_release();
}

XS_INTERNAL(cpl_xs_string)
{
    dXSARGS;

    if (items != 1)
        croak_xs_usage(cv, "value");

    SV *str = sv_newmortal();
    sv_copypv(str, ST(0));

    ST(0) = str;
    XSRETURN(1);
}

/* The string conversion of an exception object can die, outside of any eval
 * that would end the process: it is called inside an eval. Returns a new SV. */
static SV *cpl_error_string(pTHX_ SV *err)
{
    if (!SvROK(err))
        return newSVsv(err);

    SV *str = NULL;

    dSP;
    ENTER;
    SAVETMPS;
    save_scalar(PL_errgv);

    PUSHMARK(SP);
    XPUSHs(err);
    PUTBACK;

    int count = call_sv((SV *)cpl_string_cv, G_SCALAR | G_EVAL);

    SPAGAIN;
    if (count == 1) {
        SV *ret = POPs;
        if (!SvROK(ERRSV) && !SvTRUE(ERRSV))
            str = newSVsv(ret);
    }
    PUTBACK;

    FREETMPS;
    LEAVE;

    if (str == NULL)
        str = newSVpvf("%s object", sv_reftype(SvRV(err), TRUE));

    return str;
}

static void cpl_log_exception(pTHX_ const char *context, const char *identifier, SV *err,
                              bool to_stderr)
{
    bool ncollectd_error = false;
    SV *message = err;

    if (sv_isobject(err) && sv_derived_from(err, CPL_CLASS_ERROR)) {
        ncollectd_error = true;
        if (SvTYPE(SvRV(err)) == SVt_PVHV) {
            SV *sv = cpl_object_get(aTHX_ (HV *)SvRV(err), "message");
            if (sv != NULL)
                message = sv;
        }
    }

    message = sv_2mortal(cpl_error_string(aTHX_ message));

    STRLEN len = 0;
    const char *str = SvPV(message, len);
    /* Remove the new line at the end of the die messages. */
    while ((len > 0) && (str[len - 1] == '\n'))
        len--;

    if (to_stderr) {
        /* Like in the python plugin, do not log the errors of the log callbacks. */
        fprintf(stderr, "perl plugin: Unhandled perl exception in %s '%s': %.*s\n",
                context, identifier, (int)len, str);
    } else if (ncollectd_error) {
        /* NCollectd::Error is the equivalent of the python NCollectdError. */
        PLUGIN_WARNING("%s in %s '%s': %.*s", CPL_CLASS_ERROR, context, identifier,
                       (int)len, str);
    } else {
        PLUGIN_ERROR("Unhandled perl exception in %s '%s': %.*s", context, identifier,
                     (int)len, str);
    }
}

/* Calls the perl callback with the arguments in "args" followed by the data
 * given when the callback was registered. Returns -1 if the callback dies.
 * The callback can unregister itself, "c" must not be used after the call. */
static int cpl_callback_call(pTHX_ cpl_callback_t *c, const char *context, SV **args,
                             size_t nargs)
{
    char identifier[256];
    ssnprintf(identifier, sizeof(identifier), "perl/%s", c->name);
    bool is_log = c->type == CPL_CALLBACK_LOG;

    dSP;
    ENTER;
    SAVETMPS;
    /* local $@ */
    save_scalar(PL_errgv);

    SV *callback = sv_2mortal(SvREFCNT_inc_simple_NN(c->callback));
    SV *data = (c->data != NULL) ? sv_mortalcopy(c->data) : NULL;

    PUSHMARK(SP);
    EXTEND(SP, (SSize_t)nargs + 1);
    for (size_t i = 0; i < nargs; i++)
        PUSHs(args[i]);
    if (data != NULL)
        PUSHs(data);
    PUTBACK;

    call_sv(callback, G_VOID | G_DISCARD | G_EVAL);

    int status = 0;
    SV *err = ERRSV;
    if (SvROK(err) || SvTRUE(err)) {
        cpl_log_exception(aTHX_ context, identifier, err, is_log);
        status = -1;
    }

    FREETMPS;
    LEAVE;

    return status;
}

/* Calls the init, shutdown or config callbacks, which can unregister callbacks. */
static void cpl_callback_list_call(pTHX_ cpl_callback_t **list, const char *context)
{
    size_t num = 0;
    for (cpl_callback_t *c = *list; c != NULL; c = c->next)
        num++;

    if (num == 0)
        return;

    cpl_callback_t **snapshot = calloc(num, sizeof(*snapshot));
    if (snapshot == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return;
    }

    size_t n = 0;
    for (cpl_callback_t *c = *list; (c != NULL) && (n < num); c = c->next)
        snapshot[n++] = c;

    for (size_t i = 0; i < n; i++) {
        /* Skip the callbacks unregistered by a previous callback. */
        bool registered = false;
        for (cpl_callback_t *c = *list; c != NULL; c = c->next) {
            if (c == snapshot[i]) {
                registered = true;
                break;
            }
        }

        if (registered)
            cpl_callback_call(aTHX_ snapshot[i], context, NULL, 0);
    }

    free(snapshot);
}

static int cpl_read_callback(user_data_t *ud)
{
    cpl_callback_t *c = ud->data;

    PerlInterpreter *perl = cpl_acquire(&c->cancelled);
    if (perl == NULL)
        return 0;
    dTHXa(perl);

    int status = cpl_callback_call(aTHX_ c, "read callback", NULL, 0);

    cpl_release();
    return status;
}

static int cpl_write_callback(metric_family_t const *fam, user_data_t *ud)
{
    cpl_callback_t *c = ud->data;

    if (fam->name == NULL)
        return 0;

    PerlInterpreter *perl = cpl_acquire(&c->cancelled);
    if (perl == NULL)
        return 0;
    dTHXa(perl);

    ENTER;
    SAVETMPS;

    SV *arg = sv_2mortal(cpl_metric_family_to_sv(aTHX_ fam));
    int status = cpl_callback_call(aTHX_ c, "write callback", &arg, 1);

    FREETMPS;
    LEAVE;

    cpl_release();
    return status;
}

static int cpl_notification_callback(const notification_t *n, user_data_t *ud)
{
    cpl_callback_t *c = ud->data;

    if (n->name == NULL)
        return 0;

    PerlInterpreter *perl = cpl_acquire(&c->cancelled);
    if (perl == NULL)
        return 0;
    dTHXa(perl);

    ENTER;
    SAVETMPS;

    SV *arg = sv_2mortal(cpl_notification_to_sv(aTHX_ n));
    int status = cpl_callback_call(aTHX_ c, "notification callback", &arg, 1);

    FREETMPS;
    LEAVE;

    cpl_release();
    return status;
}

static void cpl_log_deliver(pTHX_ cpl_callback_t *c, int severity, const char *message)
{
    /* The messages logged by the log callbacks are not sent to the log callbacks. */
    if (cpl_log_depth > 0)
        return;
    cpl_log_depth++;

    ENTER;
    SAVETMPS;

    SV *args[2] = { sv_2mortal(newSViv(severity)), sv_2mortal(newSVpv(message, 0)) };
    cpl_callback_call(aTHX_ c, "log callback", args, STATIC_ARRAY_SIZE(args));

    FREETMPS;
    LEAVE;

    cpl_log_depth--;
}

/* The log callbacks are called from any thread, which can hold locks or be
 * waited for by the owner of the interpreter: if another thread owns the
 * interpreter the message is queued and delivered when it is released. */
static void cpl_log_callback(const log_msg_t *msg, user_data_t *ud)
{
    cpl_callback_t *c = ud->data;

    pthread_mutex_lock(&cpl_lock);

    if ((cpl_perl == NULL) || c->cancelled) {
        pthread_mutex_unlock(&cpl_lock);
        return;
    }

    if (cpl_busy_locked()) {
        cpl_log_pending_add_locked(c, msg->severity, msg->msg);
        pthread_mutex_unlock(&cpl_lock);
        return;
    }

    PerlInterpreter *perl = cpl_take_locked();
    pthread_mutex_unlock(&cpl_lock);
    dTHXa(perl);

    cpl_log_deliver(aTHX_ c, msg->severity, msg->msg);

    cpl_release();
}

/* The callback can be a code reference or the name of a subroutine,
 * relative to the package of the caller if it is not fully qualified. */
static CV *cpl_sv_to_cv(pTHX_ SV *sv)
{
    SvGETMAGIC(sv);

    if (SvROK(sv) && (SvTYPE(SvRV(sv)) == SVt_PVCV))
        return (CV *)SvRV(sv);

    if (!SvOK(sv) || SvROK(sv))
        croak("callback must be a code reference or the name of a subroutine");

    STRLEN len = 0;
    const char *name = SvPV_nomg_const(sv, len);
    U32 flags = SvUTF8(sv) ? SVf_UTF8 : 0;

    CV *cv = NULL;
    if ((strstr(name, "::") == NULL) && (strchr(name, '\'') == NULL)) {
        const char *package = CopSTASHPV(PL_curcop);
        SV *fullname = sv_2mortal(newSVpvf("%s::%s", package != NULL ? package : "main", name));
        name = SvPVX(fullname);
        cv = get_cvn_flags(name, SvCUR(fullname), flags);
    } else {
        cv = get_cvn_flags(name, len, flags);
    }

    if (cv == NULL)
        croak("Undefined subroutine '%s'", name);

    return cv;
}

/* Like in the python plugin, the default name of a callback is the name of
 * the package where it was defined (its "__module__"). */
static void cpl_callback_build_name(pTHX_ char *buffer, size_t size, CV *cv, SV *name)
{
    if (name != NULL) {
        sstrncpy(buffer, cpl_sv_to_string(aTHX_ name, "name"), size);
        return;
    }

    HV *stash = CvSTASH(cv);
    const char *package = (stash != NULL) ? HvNAME(stash) : NULL;
    if (package != NULL)
        sstrncpy(buffer, package, size);
    else
        ssnprintf(buffer, size, "%p", (void *)cv);
}

static void cpl_callback_append(cpl_callback_t **list, cpl_callback_t *c)
{
    while (*list != NULL)
        list = &(*list)->next;
    *list = c;
}

/* NCollectd::register_TYPE(callback[, data][, name]) -> identifier
 * NCollectd::register_read(callback[, interval][, data][, name]) -> identifier */
XS_INTERNAL(cpl_xs_register)
{
    dXSARGS;
    cpl_callback_type_t type = (cpl_callback_type_t)XSANY.any_i32;

    I32 max_items = (type == CPL_CALLBACK_READ) ? 4 : 3;
    if ((items < 1) || (items > max_items))
        croak_xs_usage(cv, type == CPL_CALLBACK_READ ? "callback, [interval], [data], [name]"
                                                     : "callback, [data], [name]");

    I32 idx = 1;
    double interval = 0;
    if (type == CPL_CALLBACK_READ) {
        if ((items > idx) && SvOK(ST(idx)))
            interval = cpl_sv_to_double(aTHX_ ST(idx), "interval");
        idx++;
    }

    SV *data = ((items > idx) && SvOK(ST(idx))) ? ST(idx) : NULL;
    idx++;
    SV *name = ((items > idx) && SvOK(ST(idx))) ? ST(idx) : NULL;

    CV *code = cpl_sv_to_cv(aTHX_ ST(0));

    char buffer[256];
    cpl_callback_build_name(aTHX_ buffer, sizeof(buffer), code, name);

    SV *identifier = sv_2mortal(newSVpvf("perl/%s", buffer));

    cpl_callback_t *c = cpl_callback_new(aTHX_ type, buffer, code, data);
    user_data_t ud = { .data = c, .free_func = cpl_callback_destroy };

    /* If the registration fails the daemon calls the free_func of the user data. */
    int status = 0;
    switch (type) {
    case CPL_CALLBACK_CONFIG:
        cpl_callback_append(&cpl_config_callbacks, c);
        break;
    case CPL_CALLBACK_INIT:
        cpl_callback_append(&cpl_init_callbacks, c);
        break;
    case CPL_CALLBACK_SHUTDOWN:
        cpl_callback_append(&cpl_shutdown_callbacks, c);
        break;
    case CPL_CALLBACK_READ:
        if ((interval < 0) || isnan(interval))
            interval = 0;
        c->next = cpl_daemon_callbacks;
        cpl_daemon_callbacks = c;
        status = plugin_register_complex_read("perl", buffer, cpl_read_callback,
                                              DOUBLE_TO_CDTIME_T(interval), &ud);
        break;
    case CPL_CALLBACK_WRITE:
        c->next = cpl_daemon_callbacks;
        cpl_daemon_callbacks = c;
        status = plugin_register_write("perl", buffer, cpl_write_callback, NULL, 0, 0, &ud);
        break;
    case CPL_CALLBACK_LOG:
        c->next = cpl_daemon_callbacks;
        cpl_daemon_callbacks = c;
        status = plugin_register_log("perl", buffer, cpl_log_callback, &ud);
        break;
    case CPL_CALLBACK_NOTIFICATION:
        c->next = cpl_daemon_callbacks;
        cpl_daemon_callbacks = c;
        status = plugin_register_notification("perl", buffer, cpl_notification_callback, &ud);
        break;
    }

    if (status != 0)
        croak("Unable to register the %s callback '%s'.", cpl_callback_type_name(type),
              SvPV_nolen(identifier));

    ST(0) = identifier;
    XSRETURN(1);
}

/* NCollectd::unregister_TYPE(identifier | callback) */
XS_INTERNAL(cpl_xs_unregister)
{
    dXSARGS;
    cpl_callback_type_t type = (cpl_callback_type_t)XSANY.any_i32;

    if (items != 1)
        croak_xs_usage(cv, "identifier");

    char buffer[256];
    const char *name = NULL;

    SV *arg = ST(0);
    SvGETMAGIC(arg);
    if (SvROK(arg) && (SvTYPE(SvRV(arg)) == SVt_PVCV)) {
        cpl_callback_build_name(aTHX_ buffer, sizeof(buffer), (CV *)SvRV(arg), NULL);
        name = buffer;
    } else {
        name = cpl_sv_to_string(aTHX_ arg, "identifier");
    }

    if (strncmp(name, "perl/", strlen("perl/")) == 0)
        name += strlen("perl/");

    char identifier[512];
    ssnprintf(identifier, sizeof(identifier), "perl/%s", name);

    cpl_callback_t **list = NULL;
    switch (type) {
    case CPL_CALLBACK_CONFIG:
        list = &cpl_config_callbacks;
        break;
    case CPL_CALLBACK_INIT:
        list = &cpl_init_callbacks;
        break;
    case CPL_CALLBACK_SHUTDOWN:
        list = &cpl_shutdown_callbacks;
        break;
    case CPL_CALLBACK_READ:
    case CPL_CALLBACK_WRITE:
    case CPL_CALLBACK_LOG:
    case CPL_CALLBACK_NOTIFICATION:
        break;
    }

    if (list != NULL) {
        for (cpl_callback_t **ptr = list; *ptr != NULL; ptr = &(*ptr)->next) {
            cpl_callback_t *c = *ptr;
            if (strcmp(c->name, name) == 0) {
                *ptr = c->next;
                cpl_callback_free(aTHX_ c);
                XSRETURN_EMPTY;
            }
        }
        croak("Unable to unregister the %s callback '%s'.", cpl_callback_type_name(type),
              identifier);
    }

    /* The thread that calls the callback can be waiting for the interpreter,
     * which is owned by this thread: plugin_unregister_write would wait for
     * it forever. */
    for (cpl_callback_t *c = cpl_daemon_callbacks; c != NULL; c = c->next) {
        if ((c->type == type) && (strcmp(c->name, name) == 0)) {
            cpl_callback_cancel(c);
            break;
        }
    }

    int status = -1;
    switch (type) {
    case CPL_CALLBACK_READ:
        status = plugin_unregister_read(identifier);
        break;
    case CPL_CALLBACK_WRITE:
        status = plugin_unregister_write(identifier);
        break;
    case CPL_CALLBACK_LOG:
        status = plugin_unregister_log(identifier);
        break;
    case CPL_CALLBACK_NOTIFICATION:
        status = plugin_unregister_notification(identifier);
        break;
    case CPL_CALLBACK_CONFIG:
    case CPL_CALLBACK_INIT:
    case CPL_CALLBACK_SHUTDOWN:
        break;
    }

    if (status != 0)
        croak("Unable to unregister the %s callback '%s'.", cpl_callback_type_name(type),
              identifier);

    XSRETURN_EMPTY;
}

/* NCollectd::error(message, ...), NCollectd::warning, ... */
XS_INTERNAL(cpl_xs_log)
{
    dXSARGS;
    int level = XSANY.any_i32;

    if (items < 1)
        croak_xs_usage(cv, "message, ...");

#ifndef NCOLLECTD_DEBUG
    if (level >= LOG_DEBUG)
        XSRETURN_EMPTY;
#endif

    SV *message = sv_2mortal(newSVpvs(""));
    for (I32 i = 0; i < items; i++)
        sv_catsv(message, ST(i));

    /* The location of the perl code that logs the message. */
    const char *func = NULL;
    const PERL_CONTEXT *cx = caller_cx(0, NULL);
    if ((cx != NULL) && (CxTYPE(cx) == CXt_SUB)) {
        GV *gv = CvGV(cx->blk_sub.cv);
        if (gv != NULL)
            func = GvNAME(gv);
    }

    plugin_log(level, CopFILE(PL_curcop), (int)CopLINE(PL_curcop), func, "%s",
               SvPV_nolen(message));

    XSRETURN_EMPTY;
}

#define CPL_EXPORT_LOG      0x01
#define CPL_EXPORT_REGISTER 0x02
#define CPL_EXPORT_METRIC   0x04
#define CPL_EXPORT_NOTIF    0x08
#define CPL_EXPORT_ALL      0xff

static const struct {
    const char *name;
    XSUBADDR_t xsub;
    I32 any;
    int tags;
} cpl_functions[] = {
    { "debug",                   cpl_xs_log,        LOG_DEBUG,                 CPL_EXPORT_LOG      },
    { "info",                    cpl_xs_log,        LOG_INFO,                  CPL_EXPORT_LOG      },
    { "notice",                  cpl_xs_log,        LOG_NOTICE,                CPL_EXPORT_LOG      },
    { "warning",                 cpl_xs_log,        LOG_WARNING,               CPL_EXPORT_LOG      },
    { "error",                   cpl_xs_log,        LOG_ERR,                   CPL_EXPORT_LOG      },
    { "register_config",         cpl_xs_register,   CPL_CALLBACK_CONFIG,       CPL_EXPORT_REGISTER },
    { "register_init",           cpl_xs_register,   CPL_CALLBACK_INIT,         CPL_EXPORT_REGISTER },
    { "register_read",           cpl_xs_register,   CPL_CALLBACK_READ,         CPL_EXPORT_REGISTER },
    { "register_write",          cpl_xs_register,   CPL_CALLBACK_WRITE,        CPL_EXPORT_REGISTER },
    { "register_log",            cpl_xs_register,   CPL_CALLBACK_LOG,          CPL_EXPORT_REGISTER },
    { "register_notification",   cpl_xs_register,   CPL_CALLBACK_NOTIFICATION, CPL_EXPORT_REGISTER },
    { "register_shutdown",       cpl_xs_register,   CPL_CALLBACK_SHUTDOWN,     CPL_EXPORT_REGISTER },
    { "unregister_config",       cpl_xs_unregister, CPL_CALLBACK_CONFIG,       CPL_EXPORT_REGISTER },
    { "unregister_init",         cpl_xs_unregister, CPL_CALLBACK_INIT,         CPL_EXPORT_REGISTER },
    { "unregister_read",         cpl_xs_unregister, CPL_CALLBACK_READ,         CPL_EXPORT_REGISTER },
    { "unregister_write",        cpl_xs_unregister, CPL_CALLBACK_WRITE,        CPL_EXPORT_REGISTER },
    { "unregister_log",          cpl_xs_unregister, CPL_CALLBACK_LOG,          CPL_EXPORT_REGISTER },
    { "unregister_notification", cpl_xs_unregister, CPL_CALLBACK_NOTIFICATION, CPL_EXPORT_REGISTER },
    { "unregister_shutdown",     cpl_xs_unregister, CPL_CALLBACK_SHUTDOWN,     CPL_EXPORT_REGISTER },
};

static const struct {
    const char *name;
    IV value;
    int tags;
} cpl_constants[] = {
    { "LOG_DEBUG",              LOG_DEBUG,                   CPL_EXPORT_LOG    },
    { "LOG_INFO",               LOG_INFO,                    CPL_EXPORT_LOG    },
    { "LOG_NOTICE",             LOG_NOTICE,                  CPL_EXPORT_LOG    },
    { "LOG_WARNING",            LOG_WARNING,                 CPL_EXPORT_LOG    },
    { "LOG_ERROR",              LOG_ERR,                     CPL_EXPORT_LOG    },
    { "NOTIF_FAILURE",          NOTIF_FAILURE,               CPL_EXPORT_NOTIF  },
    { "NOTIF_WARNING",          NOTIF_WARNING,               CPL_EXPORT_NOTIF  },
    { "NOTIF_OKAY",             NOTIF_OKAY,                  CPL_EXPORT_NOTIF  },
    { "METRIC_UNKNOWN",         METRIC_TYPE_UNKNOWN,         CPL_EXPORT_METRIC },
    { "METRIC_GAUGE",           METRIC_TYPE_GAUGE,           CPL_EXPORT_METRIC },
    { "METRIC_COUNTER",         METRIC_TYPE_COUNTER,         CPL_EXPORT_METRIC },
    { "METRIC_STATE_SET",       METRIC_TYPE_STATE_SET,       CPL_EXPORT_METRIC },
    { "METRIC_INFO",            METRIC_TYPE_INFO,            CPL_EXPORT_METRIC },
    { "METRIC_SUMMARY",         METRIC_TYPE_SUMMARY,         CPL_EXPORT_METRIC },
    { "METRIC_HISTOGRAM",       METRIC_TYPE_HISTOGRAM,       CPL_EXPORT_METRIC },
    { "METRIC_GAUGE_HISTOGRAM", METRIC_TYPE_GAUGE_HISTOGRAM, CPL_EXPORT_METRIC },
};

static const struct {
    const char *name;
    int tags;
} cpl_export_tags[] = {
    { "all",      CPL_EXPORT_ALL      },
    { "log",      CPL_EXPORT_LOG      },
    { "register", CPL_EXPORT_REGISTER },
    { "metric",   CPL_EXPORT_METRIC   },
    { "notif",    CPL_EXPORT_NOTIF    },
};

static void cpl_export(pTHX_ const char *package, const char *name)
{
    char fullname[256];

    ssnprintf(fullname, sizeof(fullname), "%s::%s", CPL_PACKAGE, name);
    CV *cv = get_cv(fullname, 0);
    if (cv == NULL)
        croak("\"%s\" is not exported by the %s module", name, CPL_PACKAGE);

    /* *package::name = \&NCollectd::name */
    ssnprintf(fullname, sizeof(fullname), "%s::%s", package, name);
    GV *gv = gv_fetchpv(fullname, GV_ADD, SVt_PVCV);
    sv_setsv_mg((SV *)gv, sv_2mortal(newRV_inc((SV *)cv)));
    GvIMPORTED_CV_on(gv);
}

static void cpl_export_tagged(pTHX_ const char *package, int tags)
{
    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_functions); i++) {
        if (cpl_functions[i].tags & tags)
            cpl_export(aTHX_ package, cpl_functions[i].name);
    }

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_constants); i++) {
        if (cpl_constants[i].tags & tags)
            cpl_export(aTHX_ package, cpl_constants[i].name);
    }
}

static int cpl_export_tags_get(const char *name)
{
    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_functions); i++) {
        if (strcmp(cpl_functions[i].name, name) == 0)
            return cpl_functions[i].tags;
    }

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_constants); i++) {
        if (strcmp(cpl_constants[i].name, name) == 0)
            return cpl_constants[i].tags;
    }

    return 0;
}

/* use NCollectd qw(:all);
 * use NCollectd qw(:log :metric register_read);
 * The equivalent of Exporter, which is a perl module. */
XS_INTERNAL(cpl_xs_import)
{
    dXSARGS;

    const char *package = CopSTASHPV(PL_curcop);
    if (package == NULL)
        package = "main";

    for (I32 i = 1; i < items; i++) {
        const char *name = SvPV_nolen(ST(i));

        if (name[0] == ':') {
            int tags = 0;
            for (size_t j = 0; j < STATIC_ARRAY_SIZE(cpl_export_tags); j++) {
                if (strcmp(cpl_export_tags[j].name, name + 1) == 0)
                    tags = cpl_export_tags[j].tags;
            }
            if (tags == 0)
                croak("\"%s\" is not defined in %%%s::EXPORT_TAGS", name + 1, CPL_PACKAGE);
            cpl_export_tagged(aTHX_ package, tags);
            continue;
        }

        if (name[0] == '&')
            name++;

        if (cpl_export_tags_get(name) == 0)
            croak("\"%s\" is not exported by the %s module", name, CPL_PACKAGE);

        cpl_export(aTHX_ package, name);
    }

    XSRETURN_EMPTY;
}

static const cpl_member_t cpl_error_members[] = {
    { "message", CPL_MEMBER_STRING, 0 },
};

static SV *cpl_error_to_string(pTHX_ SV *self)
{
    SV *message = cpl_object_get(aTHX_ (HV *)SvRV(self), "message");
    return (message != NULL) ? newSVsv(message) : newSVpvs(CPL_CLASS_ERROR);
}

/* die NCollectd::Error->new("message"): like the python NCollectdError, it is
 * logged as a warning. */
static const cpl_class_t cpl_error_class = {
    .name        = CPL_CLASS_ERROR,
    .base        = NULL,
    .usage       = "[message]",
    .members     = cpl_error_members,
    .members_num = STATIC_ARRAY_SIZE(cpl_error_members),
    .own_members = STATIC_ARRAY_SIZE(cpl_error_members),
    .to_string   = cpl_error_to_string,
};

/* Bootstraps the NCollectd module in the interpreter. */
static void cpl_xs_init(pTHX)
{
    static const char file[] = __FILE__;
    char name[256];

    dXSUB_SYS;

    /* enable usage of Perl modules using shared libraries */
    newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

    HV *stash = gv_stashpv(CPL_PACKAGE, GV_ADD);

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_functions); i++) {
        ssnprintf(name, sizeof(name), "%s::%s", CPL_PACKAGE, cpl_functions[i].name);
        CV *cv = newXS(name, cpl_functions[i].xsub, file);
        CvXSUBANY(cv).any_i32 = cpl_functions[i].any;
    }

    for (size_t i = 0; i < STATIC_ARRAY_SIZE(cpl_constants); i++)
        newCONSTSUB(stash, cpl_constants[i].name, newSViv(cpl_constants[i].value));

    newXS(CPL_PACKAGE "::import", cpl_xs_import, file);

    cpl_string_cv = newXS(NULL, cpl_xs_string, file);

    /* The module is built in: "use NCollectd" must not look for it in @INC. */
    (void)hv_stores(GvHVn(PL_incgv), CPL_PACKAGE ".pm", newSVpv(file, 0));

    cpl_class_boot(aTHX_ &cpl_error_class);
    cpl_config_boot(aTHX);
    cpl_metric_boot(aTHX);
    cpl_metric_family_boot(aTHX);
    cpl_notification_boot(aTHX);
}

static int cpl_interpreter_create(void)
{
    if (cpl_perl != NULL)
        return 0;

    PLUGIN_INFO("Initializing Perl interpreter...");

    cpl_argv = calloc(cpl_options_num + 4, sizeof(*cpl_argv));
    if (cpl_argv == NULL) {
        PLUGIN_ERROR("calloc failed.");
        return -1;
    }

    cpl_argc = 0;
    cpl_argv[cpl_argc++] = cpl_argv0;
    for (size_t i = 0; i < cpl_options_num; i++)
        cpl_argv[cpl_argc++] = cpl_options[i];
    cpl_argv[cpl_argc++] = cpl_argv_e;
    cpl_argv[cpl_argc++] = cpl_argv_code;
    cpl_argv[cpl_argc] = NULL;

    /* PERL_SYS_INIT3 must be called only once per process and PERL_SYS_TERM
     * after the last interpreter is destroyed, as the plugin can create a new
     * interpreter after the shutdown PERL_SYS_TERM is never called. */
    if (!cpl_sys_init) {
        int argc = cpl_argc;
        char **argv = cpl_argv;
        char **env = environ;
        PERL_SYS_INIT3(&argc, &argv, &env);
        cpl_sys_init = true;
    }

    PerlInterpreter *perl = perl_alloc();
    if (perl == NULL) {
        PLUGIN_ERROR("perl_alloc failed.");
        return -1;
    }

    PERL_SET_CONTEXT(perl);
    dTHXa(perl);

    perl_construct(perl);

    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;

    if (perl_parse(perl, cpl_xs_init, cpl_argc, cpl_argv, NULL) != 0) {
        PLUGIN_ERROR("Unable to bootstrap the perl interpreter: %s", SvPV_nolen(ERRSV));
        perl_destruct(perl);
        perl_free(perl);
        return -1;
    }

    /* Set $0 to "ncollectd" because perl_parse() has to set it to "-e". */
    sv_setpv(get_sv("0", 0), "ncollectd");

    if (perl_run(perl) != 0) {
        PLUGIN_ERROR("Unable to run the perl interpreter: %s", SvPV_nolen(ERRSV));
        perl_destruct(perl);
        perl_free(perl);
        return -1;
    }

    pthread_mutex_lock(&cpl_lock);
    cpl_perl = perl;
    cpl_shutdown_triggered = false;
    pthread_mutex_unlock(&cpl_lock);

    return 0;
}

static int cpl_option_add(const char *prefix, const char *value)
{
    char **tmp = realloc(cpl_options, sizeof(*cpl_options) * (cpl_options_num + 1));
    if (tmp == NULL) {
        PLUGIN_ERROR("realloc failed.");
        return -1;
    }
    cpl_options = tmp;

    cpl_options[cpl_options_num] = ssnprintf_alloc("%s%s", prefix, value);
    if (cpl_options[cpl_options_num] == NULL) {
        PLUGIN_ERROR("ssnprintf_alloc failed.");
        return -1;
    }
    cpl_options_num++;

    return 0;
}

/* include-dir "/path/to/perl/modules" */
static int cpl_config_include_dir(config_item_t *ci)
{
    char *dir = NULL;
    if (cf_util_get_string(ci, &dir) != 0)
        return -1;

    int status = 0;
    if (cpl_perl == NULL) {
        status = cpl_option_add("-I", dir);
    } else {
        PerlInterpreter *perl = cpl_acquire(NULL);
        if (perl != NULL) {
            dTHXa(perl);
            /* prepend the directory to @INC */
            AV *inc = GvAVn(PL_incgv);
            av_unshift(inc, 1);
            av_store(inc, 0, newSVpv(dir, 0));
            cpl_release();
        }
    }

    free(dir);
    return status;
}

/* enable-debugger "package"|"" */
static int cpl_config_enable_debugger(config_item_t *ci)
{
    char *value = NULL;
    if (cf_util_get_string(ci, &value) != 0)
        return -1;

    if (cpl_perl != NULL) {
        PLUGIN_WARNING("'enable-debugger' in %s:%d has no effect if it is used after "
                       "'load-plugin'.", cf_get_file(ci), cf_get_lineno(ci));
        free(value);
        return 0;
    }

    int status = cpl_option_add(value[0] == '\0' ? "-d" : "-d:", value);

    free(value);
    return status;
}

static bool cpl_module_name_valid(const char *name)
{
    bool start = true;

    for (const char *ptr = name; *ptr != '\0'; ptr++) {
        if (start) {
            if (!isalpha((unsigned char)*ptr) && (*ptr != '_'))
                return false;
            start = false;
        } else if ((ptr[0] == ':') && (ptr[1] == ':')) {
            ptr++;
            start = true;
        } else if (!isalnum((unsigned char)*ptr) && (*ptr != '_')) {
            return false;
        }
    }

    return !start;
}

/* load-plugin "Module::Name" */
static int cpl_config_load_plugin(config_item_t *ci)
{
    char *value = NULL;
    if (cf_util_get_string(ci, &value) != 0)
        return -1;

    char module[1024];
    if ((cpl_base_name != NULL) && (cpl_base_name[0] != '\0'))
        ssnprintf(module, sizeof(module), "%s::%s", cpl_base_name, value);
    else
        sstrncpy(module, value, sizeof(module));
    free(value);

    if (!cpl_module_name_valid(module)) {
        PLUGIN_ERROR("Invalid perl module name '%s' in %s:%d.", module,
                     cf_get_file(ci), cf_get_lineno(ci));
        return -1;
    }

    /* require Module::Name, the file is Module/Name.pm */
    char file[sizeof(module) + 3];
    size_t len = 0;
    for (const char *ptr = module; *ptr != '\0'; ptr++) {
        if ((ptr[0] == ':') && (ptr[1] == ':')) {
            file[len++] = '/';
            ptr++;
        } else {
            file[len++] = *ptr;
        }
    }
    sstrncpy(file + len, ".pm", sizeof(file) - len);

    if (cpl_interpreter_create() != 0)
        return -1;

    PerlInterpreter *perl = cpl_acquire(NULL);
    if (perl == NULL)
        return -1;
    dTHXa(perl);

    PLUGIN_DEBUG("Loading perl module '%s'.", module);

    int status = 0;

    ENTER;
    SAVETMPS;

    require_pv(file);
    if (SvROK(ERRSV) || SvTRUE(ERRSV)) {
        SV *message = sv_2mortal(cpl_error_string(aTHX_ ERRSV));
        STRLEN err_len = 0;
        const char *err = SvPV(message, err_len);
        while ((err_len > 0) && (err[err_len - 1] == '\n'))
            err_len--;
        PLUGIN_ERROR("Unable to load the perl module '%s': %.*s", module, (int)err_len, err);
        status = -1;
    }

    FREETMPS;
    LEAVE;

    cpl_release();
    return status;
}

/* plugin "name" { ... } */
static int cpl_config_plugin(config_item_t *ci)
{
    char *name = NULL;
    if (cf_util_get_string(ci, &name) != 0)
        return -1;

    PerlInterpreter *perl = cpl_acquire(NULL);
    if (perl == NULL) {
        PLUGIN_ERROR("A 'plugin' block was found in %s:%d, but no perl module was loaded "
                     "yet. Put the appropriate 'load-plugin' option in front of it.",
                     cf_get_file(ci), cf_get_lineno(ci));
        free(name);
        return -1;
    }
    dTHXa(perl);

    cpl_callback_t *c = NULL;
    for (c = cpl_config_callbacks; c != NULL; c = c->next) {
        if (strcasecmp(c->name, name) == 0)
            break;
    }

    int status = 0;
    if (c == NULL) {
        PLUGIN_WARNING("Found a configuration for the '%s' plugin, but the plugin isn't "
                       "loaded or didn't register a configuration callback.", name);
    } else {
        ENTER;
        SAVETMPS;

        SV *config = sv_2mortal(cpl_config_to_sv(aTHX_ ci, NULL));
        status = cpl_callback_call(aTHX_ c, "config callback", &config, 1);

        FREETMPS;
        LEAVE;
    }

    cpl_release();
    free(name);
    return status;
}

static int cpl_config(config_item_t *ci)
{
    int status = 0;

    for (int i = 0; i < ci->children_num; i++) {
        config_item_t *child = ci->children + i;

        if (strcasecmp(child->key, "include-dir") == 0) {
            status = cpl_config_include_dir(child);
        } else if (strcasecmp(child->key, "base-name") == 0) {
            status = cf_util_get_string(child, &cpl_base_name);
        } else if (strcasecmp(child->key, "enable-debugger") == 0) {
            status = cpl_config_enable_debugger(child);
        } else if (strcasecmp(child->key, "load-plugin") == 0) {
            status = cpl_config_load_plugin(child);
        } else if (strcasecmp(child->key, "plugin") == 0) {
            status = cpl_config_plugin(child);
        } else {
            PLUGIN_ERROR("Option '%s' in %s:%d is not allowed.",
                         child->key, cf_get_file(child), cf_get_lineno(child));
            status = -1;
        }

        if (status != 0)
            return -1;
    }

    return 0;
}

static int cpl_init(void)
{
    if (cpl_perl == NULL) {
        PLUGIN_WARNING("Plugin loaded but not configured.");
        return 0;
    }

    PerlInterpreter *perl = cpl_acquire(NULL);
    if (perl == NULL)
        return 0;
    dTHXa(perl);

    cpl_callback_list_call(aTHX_ &cpl_init_callbacks, "init callback");

    cpl_release();
    return 0;
}

static int cpl_shutdown(void)
{
    PerlInterpreter *perl = cpl_acquire(NULL);
    if (perl != NULL) {
        dTHXa(perl);

        cpl_callback_list_call(aTHX_ &cpl_shutdown_callbacks, "shutdown callback");

        cpl_callback_list_free(aTHX_ &cpl_config_callbacks);
        cpl_callback_list_free(aTHX_ &cpl_init_callbacks);
        cpl_callback_list_free(aTHX_ &cpl_shutdown_callbacks);

        cpl_shutdown_triggered = true;

        /* The interpreter is destroyed here, or when the daemon frees the
         * last read, write, log or notification callback. */
        cpl_release();
    }

    free(cpl_base_name);
    cpl_base_name = NULL;

    return 0;
}

void module_register(void)
{
    plugin_register_config("perl", cpl_config);
    plugin_register_init("perl", cpl_init);
    plugin_register_shutdown("perl", cpl_shutdown);
}
