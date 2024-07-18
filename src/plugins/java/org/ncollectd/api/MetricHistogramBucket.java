// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

public class MetricHistogramBucket
{
    protected long _counter;
    protected double _maximum;

    public MetricHistogramBucket(long counter, double maximum)
    {
        this._counter = counter;
        this._maximum = maximum;
    }

    public double getMaximum()
    {
        return this._maximum;
    }

    public long getCounter()
    {
        return this._counter;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();

        sb.append("(counter: ").append(this._counter);
        sb.append(",maximum: ").append(this._maximum).append(")");

        return sb.toString();
    }
}
