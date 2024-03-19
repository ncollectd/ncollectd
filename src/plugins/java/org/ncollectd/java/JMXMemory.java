// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.java;

import java.util.List;
import java.util.Date;
import java.util.Hashtable;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryUsage;
import java.lang.management.MemoryMXBean;

import javax.management.MBeanServerConnection;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;

import org.ncollectd.api.NCollectd;
import org.ncollectd.api.MetricFamily;
import org.ncollectd.api.MetricGauge;
import org.ncollectd.api.Notification;
import org.ncollectd.api.ConfigItem;

import org.ncollectd.api.NCollectdConfigInterface;
import org.ncollectd.api.NCollectdInitInterface;
import org.ncollectd.api.NCollectdReadInterface;
import org.ncollectd.api.NCollectdShutdownInterface;

import org.ncollectd.api.ConfigValue;
import org.ncollectd.api.ConfigItem;

public class JMXMemory implements NCollectdConfigInterface,
                                  NCollectdInitInterface,
                                  NCollectdReadInterface,
                                  NCollectdShutdownInterface
{
    private String _jmx_service_url = null;
    private MemoryMXBean _mbean = null;

    private MetricFamily fam_memory_init_bytes = new MetricFamily(MetricFamily.METRIC_TYPE_GAUGE, "jmx_memory_init_bytes");
    private MetricFamily fam_memory_used_bytes = new MetricFamily(MetricFamily.METRIC_TYPE_GAUGE, "jmx_memory_used_bytes");
    private MetricFamily fam_memory_committed_bytes = new MetricFamily(MetricFamily.METRIC_TYPE_GAUGE,  "jmx_memory_committed_bytes");
    private MetricFamily fam_memory_max_bytes = new MetricFamily(MetricFamily.METRIC_TYPE_GAUGE, "jmx_memory_max_bytes");

    public JMXMemory ()
    {
        NCollectd.registerConfig("JMXMemory", this);
        NCollectd.registerInit("JMXMemory", this);
        NCollectd.registerRead("JMXMemory", this);
        NCollectd.registerShutdown("JMXMemory", this);
    }

    private void submit (String instance, MemoryUsage usage)
    {
        long mem_init = usage.getInit();
        long mem_used = usage.getUsed();
        long mem_committed = usage.getCommitted();
        long mem_max = usage.getMax();

        NCollectd.logDebug ("JMXMemory plugin: instance = " + instance + "; "
                + "mem_init = " + mem_init + "; "
                + "mem_used = " + mem_used + "; "
                + "mem_committed = " + mem_committed + "; "
                + "mem_max = " + mem_max + ";");

        Hashtable<String, String> labels = new Hashtable<>();
        labels.put("instance", instance);

        if (mem_init >= 0) {
            fam_memory_init_bytes.addMetric(new MetricGauge(mem_init, labels));
            NCollectd.dispatchMetricFamily(fam_memory_init_bytes);
            fam_memory_init_bytes.clearMetrics();
        }

        if (mem_used >= 0) {
            fam_memory_used_bytes.addMetric(new MetricGauge(mem_used, labels));
            NCollectd.dispatchMetricFamily(fam_memory_used_bytes);
            fam_memory_used_bytes.clearMetrics();
        }

        if (mem_committed >= 0) {
            fam_memory_committed_bytes.addMetric(new MetricGauge(mem_committed, labels));
            NCollectd.dispatchMetricFamily(fam_memory_committed_bytes);
            fam_memory_committed_bytes.clearMetrics();
        }

        if (mem_max >= 0) {
            fam_memory_committed_bytes.addMetric(new MetricGauge(mem_max, labels));
            NCollectd.dispatchMetricFamily(fam_memory_committed_bytes);
            fam_memory_committed_bytes.clearMetrics();
        }
    }

    private int configServiceURL (ConfigItem ci)
    {
        List<ConfigValue> values;
        ConfigValue cv;

        values = ci.getValues ();
        if (values.size () != 1) {
            NCollectd.logError ("JMXMemory plugin: The JMXServiceURL option needs "
                    + "exactly one string argument.");
            return (-1);
        }

        cv = values.get (0);
        if (cv.getType () != ConfigValue.CONFIG_TYPE_STRING) {
            NCollectd.logError ("JMXMemory plugin: The JMXServiceURL option needs "
                    + "exactly one string argument.");
            return (-1);
        }

        _jmx_service_url = cv.getString ();
        return (0);
    }

    public int config (ConfigItem ci)
    {
        List<ConfigItem> children;
        int i;

        NCollectd.logDebug ("JMXMemory plugin: config: ci = " + ci + ";");

        children = ci.getChildren ();
        for (i = 0; i < children.size (); i++) {
            ConfigItem child;
            String key;

            child = children.get (i);
            key = child.getKey ();
            if (key.equalsIgnoreCase ("JMXServiceURL")) {
                configServiceURL (child);
            } else {
                NCollectd.logError ("JMXMemory plugin: Unknown config option: " + key);
            }
        }

        return (0);
    }

    public int init ()
    {
        JMXServiceURL service_url;
        JMXConnector connector;
        MBeanServerConnection connection;

        if (_jmx_service_url == null) {
            NCollectd.logError ("JMXMemory: _jmx_service_url == null");
            return (-1);
        }

        try {
            service_url = new JMXServiceURL (_jmx_service_url);
            connector = JMXConnectorFactory.connect (service_url);
            connection = connector.getMBeanServerConnection ();
            _mbean = ManagementFactory.newPlatformMXBeanProxy (connection,
                    ManagementFactory.MEMORY_MXBEAN_NAME,
                    MemoryMXBean.class);
        } catch (Exception e) {
            NCollectd.logError ("JMXMemory: Creating MBean failed: " + e);
            return (-1);
        }

        return (0);
    }

    public int read ()
    {
        if (_mbean == null) {
            NCollectd.logError ("JMXMemory: _mbean == null");
            return (-1);
        }

        submit ("heap", _mbean.getHeapMemoryUsage ());
        submit ("non_heap", _mbean.getNonHeapMemoryUsage ());

        return (0);
    }

    public int shutdown ()
    {
        System.out.print ("org.collectd.java.JMXMemory.Shutdown ();\n");
        _jmx_service_url = null;
        _mbean = null;
        return (0);
    }
}
