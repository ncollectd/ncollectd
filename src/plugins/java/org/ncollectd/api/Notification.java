// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2009 Hyperic, Inc.

package org.ncollectd.api;

import java.util.HashMap;
import java.util.Map;
import java.util.Date;

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
    private long _time;
    private int _severity;
    private HashMap<String, String> _labels = new HashMap<>();
    private HashMap<String, String> _annotations = new HashMap<>();

    public Notification ()
    {
        this._severity = FAILURE;
    }

    public Notification (String name, int severity, long time,
                         HashMap<String, String> labels, HashMap<String, String> annotations)
    {
        this._name = name;
        this._severity = severity;
        this._time = time;
        this._labels = labels;
        this._annotations = annotations;
    }

    public void setName(String name)
    {
        this._name = name;
    }

    public String getName()
    {
        return this._name;
    }

    public void setSeverity (int severity)
    {
        if ((severity == FAILURE) || (severity == WARNING) || (severity == OKAY))
            this._severity = severity;
    }

    public int getSeverity()
    {
        return this._severity;
    }

    public String getSeverityString()
    {
        switch (this._severity) {
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

    public void setTime(long time)
    {
        this._time = time;
    }

    public long getTime()
    {
        return this._time;
    }

    public void setLabels(HashMap<String, String> labels)
    {
        this._labels = labels;
    }

    public void addLabel(String name, String value)
    {
        this._labels.put(name, value);
    }

    public HashMap<String, String> getLabels()
    {
        return this._labels;
    }

    public void setAnnotations(HashMap<String, String> annotations)
    {
        this._annotations = annotations;
    }

    public void addAnnotation(String name, String value)
    {
        this._annotations.put(name, value);
    }

    public HashMap<String, String> getAnnotations()
    {
        return this._annotations;
    }

    public String toString()
    {
        StringBuffer sb = new StringBuffer();

        sb.append(_name);
        if (this._labels.isEmpty()) {
            sb.append("{}");
        } else {
            int i = 0;
            sb.append("{");
            for (Map.Entry<String, String> set : this._labels.entrySet()) {
                if (i != 0)
                    sb.append(",");
                sb.append(set.getKey()).append("=");
                sb.append("\"").append(set.getValue()).append("\"");
                i++;
            }
            sb.append("}");
        }

        if (!this._annotations.isEmpty()) {
            int i = 0;
            sb.append("{");
            for (Map.Entry<String, String> set : this._annotations.entrySet()) {
                if (i != 0)
                    sb.append(",");
                sb.append(set.getKey()).append("=");
                sb.append("\"").append(set.getValue()).append("\"");
                i++;
            }
        }

        switch (this._severity) {
        case FAILURE:
            sb.append(" FAILURE ");
        case WARNING:
            sb.append(" WARNING ");
        case OKAY:
            sb.append(" OKAY ");
        default:
            sb.append(" UNKNOWN ");
        }

        sb.append(this._time);

        return sb.toString();
    }
}
