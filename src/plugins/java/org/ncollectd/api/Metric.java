// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.Hashtable;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class Metric
{
    protected double _time;
    protected double _interval;
    protected Hashtable<String, String> _labels = new Hashtable<>();


    public Metric(Hashtable<String, String> labels)
    {
        _labels = labels;
    }

    public Metric(Hashtable<String, String> labels, double time)
    {
        _labels = labels;
        _time = time;
    }

    public Metric(Hashtable<String, String> labels, double time, double interval)
    {
        _labels = labels;
        _time = time;
        _interval = interval;
    }

    public double getTime() {
        return _time;
    }

    public void setTime(double time)
    {
        _time = time;
    }

    public double getInterval()
    {
        return _interval;
    }

    public void setName(double interval)
    {
        _interval = interval;
    }

    public Hashtable<String, String> getLabels()
    {
        return _labels;
    }

    public void setLabels(Hashtable<String, String> labels)
    {
        _labels = labels;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();
/*
        sb.append('[').append(new Date(_time)).append("] ");
        sb.append(getSource());
*/
        return sb.toString();

    }
}
