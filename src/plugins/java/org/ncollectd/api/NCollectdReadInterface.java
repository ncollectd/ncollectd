// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.api;

/**
 * Interface for objects implementing a read method.
 *
 * Objects implementing this interface can be registered with the daemon. Their
 * read method is then called periodically to acquire and submit values.
 *
 * @author Florian Forster &lt;octo at collectd.org&gt;
 * @see NCollectd#registerRead
 */
public interface NCollectdReadInterface
{
    /**
     * Callback method for read plugins.
     *
     * This method is called once every few seconds (depends on the
     * configuration of the daemon). It is supposed to gather values in
     * some way and submit them to the daemon using
     * {@link NCollectd#dispatchValues}.
     *
     * @return zero when successful, non-zero when an error occurred.
     * @see NCollectd#dispatchValues
     */
    public int read ();
}
