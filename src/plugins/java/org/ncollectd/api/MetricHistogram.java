// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;
import java.util.ArrayList;
import java.util.List;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class MetricHistogram extends Metric
{
    protected double _sum;
    protected List<MetricHistogramBucket> _buckets = new ArrayList<MetricHistogramBucket>();

    public MetricHistogram(double sum, List<MetricHistogramBucket> quantiles)
    {
        super();
        this._sum = sum;
        this._buckets = quantiles;
    }

    public MetricHistogram(double sum, List<MetricHistogramBucket> quantiles,
                           HashMap<String, String> labels)
    {
        super(labels);
        this._sum = sum;
        this._buckets = quantiles;
    }

    public MetricHistogram(double sum, List<MetricHistogramBucket> quantiles,
                           HashMap<String, String> labels, long time)
    {
        super(labels, time);
        this._sum = sum;
        this._buckets = quantiles;
    }

    public MetricHistogram(double sum, List<MetricHistogramBucket> quantiles,
                           HashMap<String, String> labels, long time, long interval)
    {
        super(labels, time, interval);
        this._sum = sum;
        this._buckets = quantiles;
    }

    public double getSum()
    {
        return this._sum;
    }

    public List<MetricHistogramBucket> getBuckets()
    {
        return this._buckets;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(super.toString());

        sb.append(",sum: ").append(this._sum);
        sb.append(",buckets: ");
        int i = 0;
        for (MetricHistogramBucket bucket: this._buckets) {
            if (i != 0)
                sb.append(",");
            sb.append(bucket.toString());
            i++;
        }

        return sb.toString();

    }
}
