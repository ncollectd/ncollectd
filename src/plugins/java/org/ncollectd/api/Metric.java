// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;
import java.util.Map;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class Metric
{
    protected double _time;
    protected double _interval;
    protected HashMap<String, String> _labels = new HashMap<>();

    public Metric()
    {
    }

    public Metric(HashMap<String, String> labels)
    {
        this._labels = labels;
    }

    public Metric(HashMap<String, String> labels, double time)
    {
        this._labels = labels;
        this._time = time;
    }

    public Metric(HashMap<String, String> labels, double time, double interval)
    {
        this._labels = labels;
        this._time = time;
        this._interval = interval;
    }

    public double getTime() {
        return this._time;
    }

    public void setTime(double time)
    {
        this._time = time;
    }

    public double getInterval()
    {
        return this._interval;
    }

    public void setInterval(double interval)
    {
        this._interval = interval;
    }

    public HashMap<String, String> getLabels()
    {
        return this._labels;
    }

    public void addLabel(String name, String value)
    {
        this._labels.put(name, value);
    }

    public void setLabels(HashMap<String, String> labels)
    {
        this._labels = labels;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();

        sb.append("labels: ");
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
        sb.append(", time: ").append(this._time);
        sb.append(", interval: ").append(this._interval);

        return sb.toString();

    }
}
