// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class MetricCounter extends Metric
{
    public static final int COUNTER_UINT64  = 0;
    public static final int COUNTER_FLOAT64 = 1;

    protected int _type;
    protected double _dvalue;
    protected long _lvalue;

    public MetricCounter(long value)
    {
        super();
        this._type = COUNTER_UINT64;
        this._lvalue = value;
    }

    public MetricCounter(long value, HashMap<String, String> labels)
    {
        super(labels);
        this._type = COUNTER_UINT64;
        this._lvalue = value;
    }

    public MetricCounter(long value, HashMap<String, String> labels, long time)
    {
        super(labels, time);
        this._type = COUNTER_UINT64;
        this._lvalue = value;
    }

    public MetricCounter(long value, HashMap<String, String> labels, long time, long interval)
    {
        super(labels, time, interval);
        this._type = COUNTER_UINT64;
        this._lvalue = value;
    }

    public MetricCounter(double value)
    {
        super();
        this._type = COUNTER_FLOAT64;
        this._dvalue = value;
    }

    public MetricCounter(double value, HashMap<String, String> labels)
    {
        super(labels);
        this._type = COUNTER_FLOAT64;
        this._dvalue = value;
    }

    public MetricCounter(double value, HashMap<String, String> labels, long time)
    {
        super(labels, time);
        this._type = COUNTER_FLOAT64;
        this._dvalue = value;
    }

    public MetricCounter(double value, HashMap<String, String> labels, long time, long interval)
    {
        super(labels, time, interval);
        this._type = COUNTER_FLOAT64;
        this._dvalue = value;
    }

    public int getType()
    {
        return this._type;
    }

    public double getDouble()
    {
        if (_type == COUNTER_FLOAT64) {
            return this._dvalue;
        }
        return 0;
    }

    public long getLong()
    {
        if (_type == COUNTER_UINT64) {
            return this._lvalue;
        }
        return 0;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(super.toString());

        if (this._type == COUNTER_UINT64) {
            sb.append(",type: uint64");
            sb.append(",value: ").append(this._lvalue);
        } else if (this._type == COUNTER_FLOAT64) {
            sb.append(",type: float64");
            sb.append(",value: ").append(this._dvalue);

        }

        return sb.toString();
    }
}
