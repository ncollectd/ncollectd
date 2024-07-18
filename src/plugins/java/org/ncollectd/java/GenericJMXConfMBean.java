// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009-2010  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>
//

package org.ncollectd.java;

import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import javax.management.MBeanServerConnection;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import org.ncollectd.api.NCollectd;
import org.ncollectd.api.MetricFamily;
import org.ncollectd.api.ConfigValue;
import org.ncollectd.api.ConfigItem;

class GenericJMXConfMBean
{
    private String _name; /* name by which this mapping is referenced */
    private String _metric_prefix;
    private ObjectName _obj_name;
    private HashMap<String, String> _labels;
    private HashMap<String, String> _labels_from;
    private List<GenericJMXConfMetric> _metrics;

    public GenericJMXConfMBean (ConfigItem ci) throws IllegalArgumentException
    {
        this._name = GenericJMX.getConfigString(ci);
        if (this._name == null)
            throw (new IllegalArgumentException ("No alias name was defined. "
                        + "MBean blocks need exactly one string argument."));

        this._obj_name = null;
        this._metrics = new ArrayList<GenericJMXConfMetric>();
        this._labels = new HashMap<String, String>();
        this._labels_from = new HashMap<String, String>();
        List<ConfigItem> children = ci.getChildren();
        Iterator<ConfigItem> iter = children.iterator();

        while (iter.hasNext()) {
            ConfigItem child = iter.next();

            NCollectd.logDebug ("GenericJMXConfMBean: child.getKey () = " + child.getKey());
            if (child.getKey().equalsIgnoreCase("object-name")) {
                String tmp = GenericJMX.getConfigString(child);
                if (tmp == null)
                    continue;

                try {
                    this._obj_name = new ObjectName(tmp);
                } catch (MalformedObjectNameException e) {
                    throw (new IllegalArgumentException("Not a valid object name: " + tmp, e));
                }
            } else if (child.getKey().equalsIgnoreCase("label")) {
                GenericJMX.getConfigLabel(child, this._labels);
            } else if (child.getKey().equalsIgnoreCase("label-from")) {
                GenericJMX.getConfigLabel(child, this._labels_from);
            } else if (child.getKey().equalsIgnoreCase("metric-prefix")) {
                String tmp = GenericJMX.getConfigString(child);
                if (tmp != null)
                    this._metric_prefix = tmp;
            } else if (child.getKey().equalsIgnoreCase("metric")) {
                this._metrics.add(new GenericJMXConfMetric(child));
            } else {
                throw new IllegalArgumentException("Unknown option: " + child.getKey());
            }
        }

        if (this._obj_name == null)
            throw new IllegalArgumentException("No object name was defined.");

        if (this._metrics.size () == 0)
            throw new IllegalArgumentException("No metric block was defined.");

    }

    public String getName()
    {
        return this._name;
    }

    public int query(MBeanServerConnection conn, String metric_prefix,
                                                 HashMap<String, String> labels)
    {
        Set<ObjectName> names;
        try {
            names = conn.queryNames(this._obj_name, /* query = */ null);
        } catch (Exception e) {
            NCollectd.logError ("GenericJMXConfMBean: queryNames failed: " + e);
            return (-1);
        }

        if (names.size() == 0) {
            NCollectd.logWarning("GenericJMXConfMBean: No MBean matched " + "the ObjectName " +
                                  this._obj_name);
        }

        String prefix = null;
        if ((metric_prefix != null) && (this._metric_prefix != null)) {
            prefix = metric_prefix + this._metric_prefix;
        } else if ((metric_prefix != null) && (this._metric_prefix == null)) {
            prefix = metric_prefix;
        } else if ((metric_prefix == null) && (this._metric_prefix != null)) {
            prefix = this._metric_prefix;
        }

        Iterator<ObjectName> iter = names.iterator();
        while (iter.hasNext()) {
            ObjectName objName = iter.next();

            NCollectd.logDebug ("GenericJMXConfMBean: objName = " + objName.toString());

            HashMap<String, String> mlabels = new HashMap<String, String>(labels);
            mlabels.putAll(this._labels);

            for (Map.Entry<String, String> entry: this._labels_from.entrySet()) {
                String name = entry.getKey();
                String propertyName = entry.getValue();
                String propertyValue = objName.getKeyProperty(propertyName);
                if (propertyValue == null) {
                    NCollectd.logError ("GenericJMXConfMBean: "
                            + "No such property in object name: " + propertyName);
                } else {
                    mlabels.put(name, propertyValue);
                }
            }

            for (int i = 0; i < this._metrics.size(); i++)
                this._metrics.get(i).query(conn, objName, prefix, mlabels);
        }

        return 0;
    }
}
