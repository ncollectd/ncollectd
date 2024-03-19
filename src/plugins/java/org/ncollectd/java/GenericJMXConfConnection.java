// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright (C) 2009-2012  Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at collectd.org>

package org.ncollectd.java;

import java.util.List;
import java.util.Map;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.HashMap;
import java.net.InetAddress;
import java.net.UnknownHostException;

import javax.management.MBeanServerConnection;

import javax.management.remote.JMXServiceURL;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;

import org.ncollectd.api.NCollectd;
import org.ncollectd.api.MetricFamily;
import org.ncollectd.api.ConfigValue;
import org.ncollectd.api.ConfigItem;

class GenericJMXConfConnection
{
    private String _username = null;
    private String _password = null;
    private String _host = null;
    private String _service_url = null;
    private HashMap<String, String> _labels = null;
    private JMXConnector _jmx_connector = null;
    private MBeanServerConnection _mbean_connection = null;
    private List<GenericJMXConfMBean> _mbeans = null;

    /*
     * private methods
     */
    private String getConfigString(ConfigItem ci)
    {
        List<ConfigValue> values = ci.getValues();
        if (values.size() != 1) {
            NCollectd.logError ("GenericJMXConfConnection: The " + ci.getKey()
                    + " configuration option needs exactly one string argument.");
            return null;
        }

        ConfigValue v = values.get(0);
        if (v.getType() != ConfigValue.CONFIG_TYPE_STRING) {
            NCollectd.logError ("GenericJMXConfConnection: The " + ci.getKey()
                    + " configuration option needs exactly one string argument.");
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

    private String getHost ()
    {
        if (this._host != null) {
            return this._host;
        }

        return null;
//        return NCollectd.getHostname(); // FIXME
    }

    private void connect ()
    {
        JMXServiceURL service_url;
        Map<String,Object> environment;

        // already connected
        if (this._jmx_connector != null) {
            return;
        }

        environment = null;
        if (this._password != null) {
            String[] credentials;

            if (this._username == null)
                this._username = new String ("monitorRole");

            credentials = new String[] { this._username, this._password };

            environment = new HashMap<String,Object> ();
            environment.put(JMXConnector.CREDENTIALS, credentials);
            environment.put(JMXConnectorFactory.PROTOCOL_PROVIDER_CLASS_LOADER,
                            this.getClass().getClassLoader());
        }

        try {
            service_url = new JMXServiceURL (this._service_url);
            this._jmx_connector = JMXConnectorFactory.connect (service_url, environment);
            this._mbean_connection = _jmx_connector.getMBeanServerConnection ();
        } catch (Exception e) {
            NCollectd.logError ("GenericJMXConfConnection: " +
                                "Creating MBean server connection failed: " + e);
            disconnect ();
            return;
        }
    }

    private void disconnect ()
    {
        try {
            if (this._jmx_connector != null)
                this._jmx_connector.close();
        } catch (Exception e) {
            // It's fine if close throws an exception
        }

        this._jmx_connector = null;
        this._mbean_connection = null;
    }

    /*
     * public methods
     *
     * <Connection>
     *   Host "tomcat0.mycompany"
     *   ServiceURL "service:jmx:rmi:///jndi/rmi://localhost:17264/jmxrmi"
     *   Collect "java.lang:type=GarbageCollector,name=Copy"
     *   Collect "java.lang:type=Memory"
     * </Connection>
     *
     */
    public GenericJMXConfConnection (ConfigItem ci) throws IllegalArgumentException
    {
        this._mbeans = new ArrayList<GenericJMXConfMBean> ();
        this._labels = new HashMap<String, String>();

        List<ConfigItem> children = ci.getChildren();
        Iterator<ConfigItem> iter = children.iterator();
        while (iter.hasNext()) {
            ConfigItem child = iter.next();

            if (child.getKey().equalsIgnoreCase("host")) {
                String tmp = getConfigString(child);
                if (tmp != null)
                    this._host = tmp;
            } else if (child.getKey().equalsIgnoreCase("user")) {
                String tmp = getConfigString(child);
                if (tmp != null)
                    this._username = tmp;
            } else if (child.getKey().equalsIgnoreCase("password")) {
                String tmp = getConfigString(child);
                if (tmp != null)
                    this._password = tmp;
            } else if (child.getKey().equalsIgnoreCase("service-url")) {
                String tmp = getConfigString(child);
                if (tmp != null)
                    this._service_url = tmp;
            } else if (child.getKey().equalsIgnoreCase("label")) {
                getConfigLabel(child, _labels);
            } else if (child.getKey().equalsIgnoreCase("collect")) {
                String tmp = getConfigString(child);
                if (tmp != null) {
                    GenericJMXConfMBean mbean;

                    mbean = GenericJMX.getMBean (tmp);
                    if (mbean == null)
                        throw (new IllegalArgumentException ("No such MBean defined: "
                                    + tmp + ". Please make sure all `MBean' blocks appear "
                                    + "before (above) all `Connection' blocks."));
                    NCollectd.logDebug ("GenericJMXConfConnection: " + this._host + ": Add " + tmp);
                    this._mbeans.add (mbean);
                }
            } else {
                throw (new IllegalArgumentException ("Unknown option: " + child.getKey ()));
            }
        }

        if (this._service_url == null)
            throw (new IllegalArgumentException ("No service URL was defined."));
        if (this._mbeans.size () == 0)
            throw (new IllegalArgumentException ("No valid collect statement present."));
    }

    public void query ()
    {
        // try to connect
        connect ();

        if (this._mbean_connection == null)
            return;

        NCollectd.logDebug ("GenericJMXConfConnection.query: "
                + "Reading " + this._mbeans.size () + " mbeans from "
                + ((this._host != null) ? this._host : "(null)"));

        for (int i = 0; i < this._mbeans.size (); i++) {
            int status = this._mbeans.get (i).query (this._mbean_connection, this._labels);
            if (status != 0) {
                disconnect ();
                return;
            }
        }
    }

    public String toString ()
    {
        return new String ("host = " + this._host + "; url = " + this._service_url);
    }
}
