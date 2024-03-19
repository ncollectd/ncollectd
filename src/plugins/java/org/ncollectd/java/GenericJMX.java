// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.java;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.TreeMap;

import org.ncollectd.api.NCollectd;
import org.ncollectd.api.NCollectdConfigInterface;
import org.ncollectd.api.NCollectdInitInterface;
import org.ncollectd.api.NCollectdReadInterface;
import org.ncollectd.api.NCollectdShutdownInterface;
import org.ncollectd.api.ConfigValue;
import org.ncollectd.api.ConfigItem;

public class GenericJMX implements NCollectdConfigInterface,
                                   NCollectdReadInterface,
                                   NCollectdShutdownInterface
{
    static private Map<String,GenericJMXConfMBean> _mbeans =
        new TreeMap<String,GenericJMXConfMBean> ();

    private List<GenericJMXConfConnection> _connections = null;

    public GenericJMX ()
    {
        NCollectd.registerConfig("GenericJMX", this);
        NCollectd.registerRead("GenericJMX", this);
        NCollectd.registerShutdown("GenericJMX", this);

        this._connections = new ArrayList<GenericJMXConfConnection> ();
    }

    public int config (ConfigItem ci)
    {
        List<ConfigItem> children;
        int i;

        NCollectd.logDebug ("GenericJMX plugin: config: ci = " + ci + ";");

        children = ci.getChildren ();
        for (i = 0; i < children.size (); i++) {
            ConfigItem child;
            String key;

            child = children.get (i);
            key = child.getKey ();
            if (key.equalsIgnoreCase ("MBean")) {
                try {
                    GenericJMXConfMBean mbean = new GenericJMXConfMBean (child);
                    putMBean (mbean);
                } catch (IllegalArgumentException e) {
                    NCollectd.logError("GenericJMX plugin: Evaluating 'MBean' block failed: " + e);
                }
            } else if (key.equalsIgnoreCase ("Connection")) {
                try {
                    GenericJMXConfConnection conn = new GenericJMXConfConnection (child);
                    this._connections.add (conn);
                } catch (IllegalArgumentException e) {
                    NCollectd.logError ("GenericJMX plugin: " +
                                        "Evaluating `Connection' block failed: " + e);
                }
            } else {
                NCollectd.logError ("GenericJMX plugin: Unknown config option: " + key);
            }
        }

        return (0);
    }

    public int read ()
    {
        for (int i = 0; i < this._connections.size (); i++) {
            try {
                this._connections.get (i).query ();
            } catch (Exception e) {
                NCollectd.logError ("GenericJMX: Caught unexpected exception: " + e);
                e.printStackTrace ();
            }
        }

        return (0);
    }

    public int shutdown ()
    {
        System.out.print ("org.collectd.java.GenericJMX.Shutdown ();\n");
        this._connections = null;
        return (0);
    }

    /*
     * static functions
     */
    static public GenericJMXConfMBean getMBean (String alias)
    {
        return (_mbeans.get (alias));
    }

    static private void putMBean (GenericJMXConfMBean mbean)
    {
        NCollectd.logDebug ("GenericJMX.putMBean: Adding " + mbean.getName ());
        _mbeans.put (mbean.getName (), mbean);
    }
}
