// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.HashMap;
import java.util.Map;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_t structure.
 */
public class MetricInfo extends Metric
{
    protected HashMap<String, String> _info;

    public MetricInfo(HashMap<String, String> info)
    {
        super();
        this._info = info;
    }

    public MetricInfo(HashMap<String, String> info, HashMap<String, String> labels)
    {
        super(labels);
        this._info = info;
    }

    public MetricInfo(HashMap<String, String> info, HashMap<String, String> labels, long time)
    {
        super(labels, time);
        this._info = info;
    }

    public MetricInfo(HashMap<String, String> info, HashMap<String, String> labels, long time,
                      long interval)
    {
        super(labels, time, interval);
        this._info = info;
    }

    public HashMap<String, String> getInfo()
    {
        return this._info;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer(super.toString());

        sb.append(",info: ");
        if (this._info.isEmpty()) {
            sb.append("{}");
        } else {
            int i = 0;
            sb.append("{");
            for (Map.Entry<String, String> set : this._info.entrySet()) {
                if (i != 0)
                    sb.append(",");
                sb.append(set.getKey()).append("=");
                sb.append("\"").append(set.getValue()).append("\"");
                i++;
            }
            sb.append("}");
        }

        return sb.toString();
    }
}
