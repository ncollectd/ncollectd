// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.api;

/**
 * Java API to internal functions of ncollectd.
 *
 * All functions in this class are {@code static}. You don't need to create an
 * object of this class (in fact, you can't). Just call these functions
 * directly.
 *
 * @author Florian Forster &lt;octo at collectd.org&gt;
 */
public class NCollectd
{
    /**
     * Constant for severity (log level) "error".
     *
     * @see CollectdLogInterface
     */
    public static final int LOG_ERR = 3;

    /**
     * Constant for severity (log level) "warning".
     *
     * @see CollectdLogInterface
     */
    public static final int LOG_WARNING = 4;

    /**
     * Constant for severity (log level) "notice".
     *
     * @see CollectdLogInterface
     */
    public static final int LOG_NOTICE  = 5;

    /**
     * Constant for severity (log level) "info".
     *
     * @see CollectdLogInterface
     */
    public static final int LOG_INFO = 6;

    /**
     * Constant for severity (log level) "debug".
     *
     * @see CollectdLogInterface
     */
    public static final int LOG_DEBUG = 7;

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_config
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdConfigInterface
     */
    native public static int registerConfig(String name, NCollectdConfigInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_init
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdInitInterface
     */
    native public static int registerInit(String name, NCollectdInitInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_read
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdReadInterface
     */
    native public static int registerRead(String name, NCollectdReadInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_read
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdReadInterface
     */
    native public static int registerRead(String name, NCollectdReadInterface object, long interval);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_write
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdWriteInterface
     */
    native public static int registerWrite(String name, NCollectdWriteInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_shutdown
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdShutdownInterface
     */
    native public static int registerShutdown(String name, NCollectdShutdownInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_log
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdLogInterface
     */
    native public static int registerLog(String name, NCollectdLogInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_register_notification
     *
     * @return Zero when successful, non-zero otherwise.
     * @see CollectdNotificationInterface
     */
    native public static int registerNotification(String name, NCollectdNotificationInterface object);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_dispatch_metric_family
     *
     * @return Zero when successful, non-zero otherwise.
     */
    native public static int dispatchMetricFamily(MetricFamily fam);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_dispatch_notification
     *
     * @return Zero when successful, non-zero otherwise.
     */
    native public static int dispatchNotification (Notification n);

    /**
     * Java representation of ncollectd/src/plugin.h:plugin_log
     */
    native private static void log (int severity, String message);

    /**
     * Yield contents of ncollectd/src/collectd.h:hostname_g
     *
     * @return The hostname as set in the ncollectd configuration.
     */
    /* native public static java.lang.String getHostname (); */

    /**
     * Prints an error message.
     */
    public static void logError (String message)
    {
        log (LOG_ERR, message);
    }

    /**
     * Prints a warning message.
     */
    public static void logWarning (String message)
    {
        log (LOG_WARNING, message);
    }

    /**
     * Prints a notice.
     */
    public static void logNotice (String message)
    {
        log (LOG_NOTICE, message);
    }

    /**
     * Prints an info message.
     */
    public static void logInfo (String message)
    {
        log (LOG_INFO, message);
    }

    /**
     * Prints a debug message.
     */
    public static void logDebug (String message)
    {
        log (LOG_DEBUG, message);
    }
}
