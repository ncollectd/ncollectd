// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Hyperic, Inc.

package org.ncollectd.api;

import java.util.Hashtable;

/**
 * Java representation of collectd/src/plugin.h:notfication_t structure.
 */
public class Notification
{
    public static final int FAILURE = 1;
    public static final int WARNING = 2;
    public static final int OKAY    = 4;

    public static String[] SEVERITY = {
        "FAILURE",
        "WARNING",
        "OKAY",
        "UNKNOWN"
    };

    private String _name;
    private double _time;
    private int _severity;
    private Hashtable<String, String> _labels = new Hashtable<>();
    private Hashtable<String, String> _annotations = new Hashtable<>();

    public Notification ()
    {
        _severity = FAILURE;
    }

    public Notification (String name, int severity, double time,
                         Hashtable<String, String> labels, Hashtable<String, String> annotations)
    {
        _name = name;
        _severity = severity;
        _time = time;
        _labels = labels;
        _annotations = annotations;
    }

    public void setName(String name)
    {
        this._name = name;
    }

    public String getName()
    {
        return _name;
    }

    public void setSeverity (int severity)
    {
        if ((severity == FAILURE) || (severity == WARNING) || (severity == OKAY))
            this._severity = severity;
    }

    public int getSeverity()
    {
        return _severity;
    }

    public String getSeverityString()
    {
        switch (_severity) {
        case FAILURE:
            return SEVERITY[0];
        case WARNING:
            return SEVERITY[1];
        case OKAY:
            return SEVERITY[2];
        default:
            return SEVERITY[3];
        }
    }

    public void setTime(double time)
    {
        this._time = time;
    }

    public double getTime()
    {
        return _time;
    }

    public void setLabels(Hashtable<String, String> labels)
    {
        this._labels = labels;
    }

    public Hashtable<String, String> getLabels()
    {
        return _labels;
    }

    public void setAnnotations(Hashtable<String, String> annotations)
    {
        this._annotations = annotations;
    }

    public Hashtable<String, String> getAnnotations()
    {
        return _annotations;
    }

    public String toString()
    {
        StringBuffer sb = new StringBuffer(super.toString());
        return sb.toString();
    }
}
