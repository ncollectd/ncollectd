// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;
import java.util.ArrayList;
import java.util.List;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class MetricSummary extends Metric
{
    protected double _sum;
    protected long _count;
    protected List<MetricSummaryQuantile> _quantiles = new ArrayList<MetricSummaryQuantile>();

    public MetricSummary(double sum, long count, List<MetricSummaryQuantile> quantiles)
    {
        super();
        this._sum = sum;
        this._count = count;
        this._quantiles = quantiles;
    }

    public MetricSummary(double sum, long count, List<MetricSummaryQuantile> quantiles,
                         HashMap<String, String> labels)
    {
        super(labels);
        this._sum = sum;
        this._count = count;
        this._quantiles = quantiles;
    }

    public MetricSummary(double sum, long count, List<MetricSummaryQuantile> quantiles,
                         HashMap<String, String> labels, long time)
    {
        super(labels, time);
        this._sum = sum;
        this._count = count;
        this._quantiles = quantiles;
    }

    public MetricSummary(double sum, long count, List<MetricSummaryQuantile> quantiles,
                         HashMap<String, String> labels, long time, long interval)
    {
        super(labels, time, interval);
        this._sum = sum;
        this._count = count;
        this._quantiles = quantiles;
    }

    public double getSum()
    {
        return _sum;
    }

    public double getCount()
    {
        return _count;
    }

    public List<MetricSummaryQuantile> getQuantiles()
    {
        return _quantiles;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(super.toString());

        sb.append(",sum: ").append(this._sum);
        sb.append(",count: ").append(this._count);
        sb.append(",quantiles: ");
        int i = 0;
        for (MetricSummaryQuantile quantile : this._quantiles) {
            if (i != 0)
                sb.append(",");
            sb.append(quantile.toString());
            i++;
        }

        return sb.toString();
    }
}
