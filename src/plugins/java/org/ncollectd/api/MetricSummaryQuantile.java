// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

public class MetricSummaryQuantile
{
    protected double _quantile;
    protected double _value;

    public MetricSummaryQuantile(double quantile, double value)
    {
        this._value = value;
        this._quantile = quantile;
    }

    public double getQuantile()
    {
        return this._quantile;
    }

    public double getValue()
    {
        return this._value;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();

        sb.append("(value: ").append(this._value);
        sb.append(",quantile: ").append(this._quantile).append(")");

        return sb.toString();
    }
}
