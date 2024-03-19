// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.api;

/**
 * Interface for objects implementing a flush method.
 *
 * @author Florian Forster &lt;octo at collectd.org&gt;
 * @see NCollectd#registerFlush
 */
public interface NCollectdFlushInterface
{
    public int flush (Number timeout, String identifier);
}
