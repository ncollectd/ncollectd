// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;
import java.util.Map;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class Metric
{
    protected long _time;
    protected long _interval;
    protected HashMap<String, String> _labels = new HashMap<>();

    public Metric()
    {
    }

    public Metric(HashMap<String, String> labels)
    {
        this._labels = labels;
    }

    public Metric(HashMap<String, String> labels, long time)
    {
        this._labels = labels;
        this._time = time;
    }

    public Metric(HashMap<String, String> labels, long time, long interval)
    {
        this._labels = labels;
        this._time = time;
        this._interval = interval;
    }

    public long getTime() {
        return this._time;
    }

    public void setTime(long time)
    {
        this._time = time;
    }

    public long getInterval()
    {
        return this._interval;
    }

    public void setInterval(long interval)
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
