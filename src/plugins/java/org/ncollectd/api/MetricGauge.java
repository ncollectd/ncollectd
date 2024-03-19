// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.Hashtable;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class MetricGauge extends Metric
{
    public static final int GAUGE_FLOAT64 = 0;
    public static final int GAUGE_INT64   = 1;

    protected int _type;
    protected double _dvalue;
    protected long _lvalue;

    public MetricGauge(long value, Hashtable<String, String> labels)
    {
        super(labels);
        _type = GAUGE_INT64;
        _lvalue = value;
    }

    public MetricGauge(long value, Hashtable<String, String> labels, double time)
    {
        super(labels, time);
        _type = GAUGE_INT64;
        _lvalue = value;
    }

    public MetricGauge(long value, Hashtable<String, String> labels, double time, double interval)
    {
        super(labels, time, interval);
        _type = GAUGE_INT64;
        _lvalue = value;
    }

    public MetricGauge(double value, Hashtable<String, String> labels)
    {
        super(labels);
        _type = GAUGE_FLOAT64;
        _dvalue = value;
    }

    public MetricGauge(double value, Hashtable<String, String> labels, double time)
    {
        super(labels, time);
        _type = GAUGE_FLOAT64;
        _dvalue = value;
    }

    public MetricGauge(double value, Hashtable<String, String> labels, double time, double interval)
    {
        super(labels, time, interval);
        _type = GAUGE_FLOAT64;
        _dvalue = value;
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
