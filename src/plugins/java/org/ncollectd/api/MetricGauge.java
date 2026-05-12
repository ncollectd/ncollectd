// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;

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

    public MetricGauge(long value)
    {
        super();
        this._type = GAUGE_INT64;
        this._lvalue = value;
    }

    public MetricGauge(long value, HashMap<String, String> labels)
    {
        super(labels);
        this._type = GAUGE_INT64;
        this._lvalue = value;
    }

    public MetricGauge(long value, HashMap<String, String> labels, double time)
    {
        super(labels, time);
        this._type = GAUGE_INT64;
        this._lvalue = value;
    }

    public MetricGauge(long value, HashMap<String, String> labels, double time, double interval)
    {
        super(labels, time, interval);
        this._type = GAUGE_INT64;
        this._lvalue = value;
    }

    public MetricGauge(double value)
    {
        super();
        this._type = GAUGE_FLOAT64;
        this._dvalue = value;
    }

    public MetricGauge(double value, HashMap<String, String> labels)
    {
        super(labels);
        this._type = GAUGE_FLOAT64;
        this._dvalue = value;
    }

    public MetricGauge(double value, HashMap<String, String> labels, double time)
    {
        super(labels, time);
        this._type = GAUGE_FLOAT64;
        this._dvalue = value;
    }

    public MetricGauge(double value, HashMap<String, String> labels, double time, double interval)
    {
        super(labels, time, interval);
        this._type = GAUGE_FLOAT64;
        this._dvalue = value;
    }

    public int getType()
    {
        return this._type;
    }

    public double getDouble()
    {
        if (this._type == GAUGE_FLOAT64) {
            return this._dvalue;
        }
        return (double)this._lvalue;
    }

    public long getLong()
    {
        if (this._type == GAUGE_INT64) {
            return this._lvalue;
        }
        return (long)this._dvalue;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(super.toString());

        if (this._type == GAUGE_FLOAT64) {
            sb.append(",type: float64");
            sb.append(",value: ").append(this._dvalue);
        } else if (this._type == GAUGE_INT64) {
            sb.append(",type: int64");
            sb.append(",value: ").append(this._lvalue);

        }

        return sb.toString();
    }
}
