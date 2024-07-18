// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;
import java.util.Map;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class MetricStateSet extends Metric
{
    protected HashMap<String, Boolean> _set;

    public MetricStateSet(HashMap<String, Boolean> set)
    {
        super();
        this._set = set;
    }

    public MetricStateSet(HashMap<String, Boolean> set, HashMap<String, String> labels)
    {
        super(labels);
        this._set = set;
    }

    public MetricStateSet(HashMap<String, Boolean> set, HashMap<String, String> labels, long time)
    {
        super(labels, time);
        this._set = set;
    }

    public MetricStateSet(HashMap<String, Boolean> set, HashMap<String, String> labels, long time,
                          long interval)
    {
        super(labels, time, interval);
        this._set = set;
    }

    public HashMap<String, Boolean> getSet()
    {
        return this._set;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(super.toString());

        sb.append(",set: ");
        if (this._set.isEmpty()) {
            sb.append("{}");
        } else {
            int i = 0;
            sb.append("{");
            for (Map.Entry<String, Boolean> set : this._set.entrySet()) {
                if (i != 0)
                    sb.append(",");
                sb.append(set.getKey()).append("=").append(set.getValue());
                i++;
            }
            sb.append("}");
        }

        return sb.toString();

    }
}
