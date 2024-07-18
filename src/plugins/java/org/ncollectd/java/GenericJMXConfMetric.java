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
import org.ncollectd.api.MetricUnknown;
import org.ncollectd.api.MetricGauge;
import org.ncollectd.api.MetricCounter;
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
 * ncollectd.
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

    private Metric genericObjectToMetric(Object obj, int type, HashMap<String, String> labels)
    {
        if (obj instanceof String) {
            String str = (String)obj;

            try {
                if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                    double value = Double.valueOf(str).doubleValue();
                    return new MetricUnknown(value, labels);
                } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                    double value = Double.valueOf(str).doubleValue();
                    return new MetricGauge(value, labels);
                } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                    double value = Long.valueOf(str).longValue();
                    return new MetricCounter(value, labels);
                }
            } catch (NumberFormatException e) {
                return null;
            }
        } else if (obj instanceof Byte) {
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(((Byte)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(((Byte)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(((Byte)obj).longValue(), labels);
            }
        } else if (obj instanceof Short) {
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(((Short)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(((Short)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(((Short)obj).longValue(), labels);
            }
        } else if (obj instanceof Integer) {
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(((Integer)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(((Integer)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(((Integer)obj).longValue(), labels);
            }
        } else if (obj instanceof Long) {
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(((Long)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(((Long)obj).longValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(((Long)obj).longValue(), labels);
            }
        } else if (obj instanceof Float) {
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(((Float)obj).doubleValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(((Float)obj).doubleValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(((Float)obj).doubleValue(), labels);
            }
        } else if (obj instanceof Double) {
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(((Double)obj).doubleValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(((Double)obj).doubleValue(), labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(((Double)obj).doubleValue(), labels);
            }
        } else if (obj instanceof BigDecimal) {
            double value = BigDecimal.ZERO.add((BigDecimal)obj).doubleValue();
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(value, labels);
            }
        } else if (obj instanceof BigInteger) {
            long value = BigInteger.ZERO.add((BigInteger)obj).longValue();
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(value, labels);
            }
        } else if (obj instanceof AtomicInteger) {
            long value = Integer.valueOf(((AtomicInteger)obj).get()).longValue();
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(value, labels);
            }
        } else if (obj instanceof AtomicLong) {
            long value = Long.valueOf(((AtomicLong)obj).get()).longValue();
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(value, labels);
            }
        } else if (obj instanceof Boolean) {
            long value = ((Boolean)obj).booleanValue() ? 1 : 0;
            if (type == MetricFamily.METRIC_TYPE_UNKNOWN) {
                return new MetricUnknown(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_GAUGE) {
                return new MetricGauge(value, labels);
            } else if (type == MetricFamily.METRIC_TYPE_COUNTER) {
                return new MetricCounter(value, labels);
            }
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
        if (value == null) {
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

        /*
         * metric "metric-name" {
         *   type "counter"
         *   table true|false
         *   attribute "HeapMemoryUsage"
         *   :
         * }
         */

        String name = GenericJMX.getConfigString(ci);
        if (name == null)
            throw new IllegalArgumentException("Missing metric name.");

        this._metric_name = name;
        // FIXME else

        List<ConfigItem> children = ci.getChildren();
        Iterator<ConfigItem> iter = children.iterator();
        while (iter.hasNext()) {
            ConfigItem child = iter.next();

            if (child.getKey().equalsIgnoreCase("type")) {
                String tmp = GenericJMX.getConfigString(child);
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
            } else if (child.getKey().equalsIgnoreCase("help")) {
                String tmp = GenericJMX.getConfigString(child);
                if (tmp != null) {
                    this._metric_help = tmp;
                }
            } else if (child.getKey().equalsIgnoreCase("unit")) {
                String tmp = GenericJMX.getConfigString(child);
                if (tmp != null) {
                    this._metric_unit = tmp;
                }
            } else if (child.getKey().equalsIgnoreCase("attribute")) {
                String tmp = GenericJMX.getConfigString(child);
                if (tmp != null) {
                    this._attribute = tmp;
                }
            } else if (child.getKey().equalsIgnoreCase("label")) {
                GenericJMX.getConfigLabel(child, this._labels);
            } else if (child.getKey().equalsIgnoreCase("label-from")) {
                GenericJMX.getConfigLabel(child, this._labels_from);
            } else {
                throw new IllegalArgumentException("Unknown option: " + child.getKey());
            }
        }

        if (this._attribute == null)
            throw new IllegalArgumentException("No attribute was defined.");
    }

    /**
     * Query values via JMX according to the object's configuration and dispatch
     * them to ncollectd.
     *
     * @param conn    Connection to the MBeanServer.
     * @param objName Object name of the MBean to query.
     * @param labels  Labes to add to the metric.
     *
     */
    public void query(MBeanServerConnection conn, ObjectName objName, String metric_prefix,
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

        if (object instanceof CompositeData) {
            CompositeData cd = (CompositeData)object;
            Set<String> set =  cd.getCompositeType().keySet();
            NCollectd.logError("GenericJMXConfMetric.query: attribute \"" +
                               this._attribute + "\" is CompositeData object ("+
                               String.join(",", set) + ")");
            return;
        }

        Metric metric = genericObjectToMetric(object, this._type, mlabels);
        if (metric == null) {
            NCollectd.logError("GenericJMXConfMetric: Cannot convert object to metric.");
            return;
        }

        String metric_name = null;
        if (metric_prefix != null) {
            metric_name = metric_prefix + this._metric_name;
        } else {
            metric_name = this._metric_name;
        }

        MetricFamily fam = new MetricFamily(this._type, metric_name,
                                            this._metric_help, this._metric_unit);
        fam.addMetric(metric);
        NCollectd.dispatchMetricFamily(fam);
    }
}
