// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.java;

import java.util.Arrays;
import java.util.List;
import java.util.Collection;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import java.math.BigDecimal;
import java.math.BigInteger;

import javax.management.MBeanServerConnection;
import javax.management.ObjectName;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.TabularData;
import javax.management.openmbean.InvalidKeyException;

import org.ncollectd.api.NCollectd;
import org.ncollectd.api.MetricFamily;
import org.ncollectd.api.Metric;
import org.ncollectd.api.ConfigValue;
import org.ncollectd.api.ConfigItem;

/**
 * Representation of a &lt;metric&nbsp;/&gt; block and query functionality.
 *
 * This class represents a &lt;metric&nbsp;/&gt; block in the configuration. As
 * such, the constructor takes an {@link org.ncollectd.api.ConfigValue} to
 * construct an object of this class.
 *
 * The object can then be asked to query data from JMX and dispatch it to
 * collectd.
 *
 * @see GenericJMXConfMBean
 */
class GenericJMXConfMetric
{
    private String _metric_name;
    private String _metric_help;
    private String _metric_unit;
    private int _type;
    private String _attribute;
    private HashMap<String, String> _labels;
    private HashMap<String, String> _labels_from;
    private boolean _is_table;

    /**
     * Converts a generic (OpenType) object to a number.
     *
     * Returns null if a conversion is not possible or not implemented.
     */
    private Number genericObjectToNumber(Object obj, int type)
    {
        if (obj instanceof String) {
            String str = (String)obj;

            try {
                if ((type == MetricFamily.METRIC_TYPE_UNKNOWN) ||
                    (type == MetricFamily.METRIC_TYPE_GAUGE)) {
                    return Double.valueOf(str);
                } else {
                    return Long.valueOf(str);
                }
            } catch (NumberFormatException e) {
                return null;
            }
        } else if (obj instanceof Byte) {
            return Byte.valueOf((Byte)obj);
        } else if (obj instanceof Short) {
            return Short.valueOf((Short)obj);
        } else if (obj instanceof Integer) {
            return Integer.valueOf((Integer)obj);
        } else if (obj instanceof Long) {
            return Long.valueOf((Long)obj);
        } else if (obj instanceof Float) {
            return Float.valueOf((Float)obj);
        } else if (obj instanceof Double) {
            return Double.valueOf((Double)obj);
        } else if (obj instanceof BigDecimal) {
            return BigDecimal.ZERO.add((BigDecimal)obj);
        } else if (obj instanceof BigInteger) {
            return BigInteger.ZERO.add((BigInteger)obj);
        } else if (obj instanceof AtomicInteger) {
            return Integer.valueOf(((AtomicInteger)obj).get());
        } else if (obj instanceof AtomicLong) {
            return Long.valueOf(((AtomicLong)obj).get());
        } else if (obj instanceof Boolean) {
            return (Boolean)obj ? 1 : 0;
        }

        return null;
    }

    private Object queryAttributeRecursive(CompositeData parent, List<String> attrName)
    {
        String key = attrName.remove(0);

        Object value;
        try {
            value = parent.get(key);
        } catch (InvalidKeyException e) {
            return null;
        }

        if (attrName.size() == 0) {
            return value;
        } else {
            if (value instanceof CompositeData)
                return queryAttributeRecursive((CompositeData)value, attrName);
            else if (value instanceof TabularData)
                return queryAttributeRecursive((TabularData)value, attrName);
            else
                return null;
        }
    }

    private Object queryAttributeRecursive(TabularData parent, List<String> attrName)
    {
        String key;
        Object value = null;

        key = attrName.remove(0);

        @SuppressWarnings("unchecked") Collection<CompositeData> table =
                (Collection<CompositeData>)parent.values();
        for (CompositeData compositeData : table) {
            if (key.equals(compositeData.get("key"))) {
                value = compositeData.get("value");
            }
        }
        if (null == value) {
            return (null);
        }

        if (attrName.size() == 0) {
            return (value);
        } else {
            if (value instanceof CompositeData)
                return (queryAttributeRecursive((CompositeData)value, attrName));
            else if (value instanceof TabularData)
                return (queryAttributeRecursive((TabularData)value, attrName));
            else
                return (null);
        }
    }

    private Object queryAttribute(MBeanServerConnection conn, ObjectName objName, String attrName)
    {
        List<String> attrNameList = new ArrayList<String>();

        String[] attrNameArray = attrName.split("\\.");
        String key = attrNameArray[0];
        for (int i = 1; i < attrNameArray.length; i++)
            attrNameList.add(attrNameArray[i]);

        Object value;
        try {
            try {
                value = conn.getAttribute(objName, key);
            } catch (javax.management.AttributeNotFoundException e) {
                value = conn.invoke(objName, key, /* args = */ null, /* types = */ null);
            }
        } catch (Exception e) {
            NCollectd.logError("GenericJMXConfMetric.query: getAttribute failed: " + e);
            return null;
        }

        if (attrNameList.size() == 0) {
            return value;
        } else {
            if (value instanceof CompositeData)
                return (queryAttributeRecursive((CompositeData)value, attrNameList));
            else if (value instanceof TabularData)
                return (queryAttributeRecursive((TabularData)value, attrNameList));
            else if (value instanceof OpenType) {
                OpenType ot = (OpenType)value;
                NCollectd.logNotice("GenericJMXConfMetric: Handling of OpenType \"" +
                                    ot.getTypeName() + "\" is not yet implemented.");
                return null;
            } else {
                NCollectd.logError("GenericJMXConfMetric: Received object of " +
                                   "unknown class. " + attrName + " " +
                                   ((value == null) ? "null" : value.getClass().getName()));
                return null;
            }
        }
    }

    private String join(String separator, List<String> list)
    {
        StringBuffer sb = new StringBuffer();

        for (int i = 0; i < list.size(); i++) {
            if (i > 0)
                sb.append("-");
            sb.append(list.get(i));
        }

        return sb.toString();
    }

    private String getConfigString(ConfigItem ci)
    {

        List<ConfigValue> values = ci.getValues();
        if (values.size() != 1) {
            NCollectd.logError("GenericJMXConfMetric: The " + ci.getKey() +
                               " configuration option needs exactly one string argument.");
            return null;
        }

        ConfigValue v = values.get(0);
        if (v.getType() != ConfigValue.CONFIG_TYPE_STRING) {
            NCollectd.logError("GenericJMXConfMetric: The " + ci.getKey() +
                               " configuration option needs exactly one string argument.");
            return null;
        }

        return v.getString();
    }

    private void getConfigLabel(ConfigItem ci, HashMap<String, String> labels)
    {
        List<ConfigValue> values = ci.getValues();
        if (values.size() != 2) {
            NCollectd.logError ("GenericJMXConfConnection: The " + ci.getKey()
                    + " configuration option needs exactly two string arguments.");
            return;
        }

        ConfigValue name = values.get(0);
        if (name.getType() != ConfigValue.CONFIG_TYPE_STRING) {
            NCollectd.logError ("GenericJMXConfConnection: The " + ci.getKey()
                    + " configuration option needs exactly two string arguments.");
            return;
        }

        ConfigValue value = values.get(1);
        if (value.getType() != ConfigValue.CONFIG_TYPE_STRING) {
            NCollectd.logError ("GenericJMXConfConnection: The " + ci.getKey()
                    + " configuration option needs exactly two string arguments.");
            return;
        }

        labels.put(name.getString(), value.getString());
    }

    private Boolean getConfigBoolean(ConfigItem ci)
    {
        List<ConfigValue> values = ci.getValues();
        if (values.size() != 1) {
            NCollectd.logError("GenericJMXConfMetric: The " + ci.getKey() +
                               " configuration option needs exactly one boolean argument.");
            return null;
        }

        ConfigValue v = values.get(0);
        if (v.getType() != ConfigValue.CONFIG_TYPE_BOOLEAN) {
            NCollectd.logError("GenericJMXConfMetric: The " + ci.getKey() +
                               " configuration option needs exactly one boolean argument.");
            return null;
        }

        return Boolean.valueOf(v.getBoolean());
    }

    /**
     * Constructs a new value with the configured properties.
     */
    public GenericJMXConfMetric(ConfigItem ci) throws IllegalArgumentException
    {
        this._metric_name = null;
        this._type = MetricFamily.METRIC_TYPE_UNKNOWN;
        this._attribute = null;
        this._labels = new HashMap<String, String>();
        this._labels_from = new HashMap<String, String>();
        this._is_table = false;

        /*
         * metric "metric-name" {
         *   type "counter"
         *   table true|false
         *   attribute "HeapMemoryUsage"
         *   attribute "..."
         *   :
         *   # Type instance:
         * }
         */

        String name = getConfigString(ci);
        if (name != null)
            this._metric_name = name;
        // FIXME else

        List<ConfigItem> children = ci.getChildren();
        Iterator<ConfigItem> iter = children.iterator();
        while (iter.hasNext()) {
            ConfigItem child = iter.next();

            if (child.getKey().equalsIgnoreCase("type")) {
                String tmp = getConfigString(child);
                if (tmp != null) {
                    if (tmp.equalsIgnoreCase("unknown")) {
                        this._type = MetricFamily.METRIC_TYPE_UNKNOWN;
                    } else if (tmp.equalsIgnoreCase("gauge")) {
                        this._type = MetricFamily.METRIC_TYPE_GAUGE;
                    } else if (tmp.equalsIgnoreCase("counter")) {
                        this._type = MetricFamily.METRIC_TYPE_COUNTER;
                    } else {
                        this._type = MetricFamily.METRIC_TYPE_UNKNOWN;
                    }
                }
            } else if (child.getKey().equalsIgnoreCase("table")) {
                Boolean tmp = getConfigBoolean(child);
                if (tmp != null)
                    this._is_table = tmp.booleanValue();
            } else if (child.getKey().equalsIgnoreCase("attribute")) {
                String tmp = getConfigString(child);
                if (tmp != null)
                    this._attribute = tmp;
            } else if (child.getKey().equalsIgnoreCase("label")) {
                getConfigLabel(child, this._labels);
            } else if (child.getKey().equalsIgnoreCase("label-from")) {
                getConfigLabel(child, this._labels_from);
            } else
                throw new IllegalArgumentException("Unknown option: " + child.getKey());
        }

        if (this._attribute == null)
            throw new IllegalArgumentException("No attribute was defined.");
    }

    /**
     * Query values via JMX according to the object's configuration and dispatch
     * them to collectd.
     *
     * @param conn      Connection to the MBeanServer.
     * @param objName Object name of the MBean to query.
     * @param pd            Preset naming components. The members host, plugin and
     *                              plugin instance will be used.
     */
    public void query(MBeanServerConnection conn, ObjectName objName,
                      HashMap<String, String> labels)
    {
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

        Object object = queryAttribute(conn, objName, this._attribute);
        if (object == null) {
            NCollectd.logError("GenericJMXConfMetric.query: " + "Querying attribute " +
                               this._attribute + " failed.");
            return;
        }

        Number value = genericObjectToNumber(object, this._type);
        if (value == null) {
            NCollectd.logError("GenericJMXConfMetric: Cannot convert object to number.");
            return;
        }

        // FIXME

        MetricFamily fam = new MetricFamily(this._type, this._metric_name,
                                            this._metric_help, this._metric_unit);
//        fam.addMetric(m);
        NCollectd.dispatchMetricFamily(fam);
//        fam.clearMetrics();
    }
}
