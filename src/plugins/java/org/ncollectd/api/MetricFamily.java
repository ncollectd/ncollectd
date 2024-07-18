// SPDX-License-Identifier: GPL-2.0-only

package org.ncollectd.api;

import java.util.ArrayList;
import java.util.List;

/**
 * Java representation of ncollectd/src/libmetric/metric.h:metric_familty_t structure.
 */
public class MetricFamily
{
    public static final int METRIC_TYPE_UNKNOWN         = 0;
    public static final int METRIC_TYPE_GAUGE           = 1;
    public static final int METRIC_TYPE_COUNTER         = 2;
    public static final int METRIC_TYPE_STATE_SET       = 3;
    public static final int METRIC_TYPE_INFO            = 4;
    public static final int METRIC_TYPE_SUMMARY         = 5;
    public static final int METRIC_TYPE_HISTOGRAM       = 6;
    public static final int METRIC_TYPE_GAUGE_HISTOGRAM = 7;

    protected String _name;
    protected String _help;
    protected String _unit;
    protected int _type;
    protected List<Metric> _metrics = new ArrayList<Metric>();

    public MetricFamily(int type, String name)
    {
        switch (type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_STATE_SET:
        case METRIC_TYPE_INFO:
        case METRIC_TYPE_SUMMARY:
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            this._type = type;
            break;
        default:
            this._type = METRIC_TYPE_UNKNOWN;
            break;
        }
        this._name = name;
    }

    public MetricFamily(int type, String name, String help)
    {
        switch (type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_STATE_SET:
        case METRIC_TYPE_INFO:
        case METRIC_TYPE_SUMMARY:
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            this._type = type;
            break;
        default:
            this._type = METRIC_TYPE_UNKNOWN;
            break;
        }
        this._name = name;
        this._help = help;
    }

    public MetricFamily(int type, String name, String help, String unit)
    {
        switch (type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_STATE_SET:
        case METRIC_TYPE_INFO:
        case METRIC_TYPE_SUMMARY:
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            this._type = type;
            break;
        default:
            this._type = METRIC_TYPE_UNKNOWN;
            break;
        }
        this._name = name;
        this._help = help;
        this._unit = unit;
    }

    public MetricFamily(int type, String name, String help, String unit, List<Metric> metrics)
    {
        switch (type) {
        case METRIC_TYPE_UNKNOWN:
        case METRIC_TYPE_GAUGE:
        case METRIC_TYPE_COUNTER:
        case METRIC_TYPE_STATE_SET:
        case METRIC_TYPE_INFO:
        case METRIC_TYPE_SUMMARY:
        case METRIC_TYPE_HISTOGRAM:
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            this._type = type;
            break;
        default:
            this._type = METRIC_TYPE_UNKNOWN;
            break;
        }
        this._name = name;
        this._help = help;
        this._unit = unit;
        this._metrics = metrics;
    }

    public int getType()
    {
        return this._type;
    }

    public String getName()
    {
        return this._name;
    }

    public void setName(String name)
    {
        this._name = name;
    }

    public String getHelp()
    {
        return this._help;
    }

    public void setHelp(String help)
    {
        this._help = help;
    }

    public String getUnit()
    {
        return this._unit;
    }

    public void setUnit(String unit)
    {
        this._unit = unit;
    }

    public List<Metric> getMetrics()
    {
        return this._metrics;
    }

    public void setMetrics(List<Metric> metrics)
    {
        this._metrics = metrics;
    }

    public void addMetric(Metric metric)
    {
        switch (this._type) {
        case METRIC_TYPE_UNKNOWN:
            if (!(metric instanceof MetricUnknown)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_GAUGE:
            if (!(metric instanceof MetricGauge)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_COUNTER:
            if (!(metric instanceof MetricCounter)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_STATE_SET:
            if (!(metric instanceof MetricStateSet)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_INFO:
            if (!(metric instanceof MetricInfo)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_SUMMARY:
            if (!(metric instanceof MetricSummary)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_HISTOGRAM:
            if (!(metric instanceof MetricHistogram)) {
                return; // FIXME throw exception ¿?
            }
            break;
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            if (!(metric instanceof MetricHistogram)) {
                return; // FIXME throw exception ¿?
            }
            break;
        default:
            return;
        }

        this._metrics.add(metric);
    }

    public void clearMetrics()
    {
        this._metrics.clear();
    }

    public String toString()
    {
        StringBuffer sb = new StringBuffer();
        sb.append("name: ").append(this._name);
        if (this._help != null)
            sb.append(",help: ").append(this._help);
        if (this._unit != null)
            sb.append(",unit: ").append(this._unit);

        sb.append(",type: ");
        switch (this._type) {
        case METRIC_TYPE_UNKNOWN:
            sb.append("unknown");
            break;
        case METRIC_TYPE_GAUGE:
            sb.append("gauge");
            break;
        case METRIC_TYPE_COUNTER:
            sb.append("counter");
            break;
        case METRIC_TYPE_STATE_SET:
            sb.append("state_set");
            break;
        case METRIC_TYPE_INFO:
            sb.append("info");
            break;
        case METRIC_TYPE_SUMMARY:
            sb.append("summary");
            break;
        case METRIC_TYPE_HISTOGRAM:
            sb.append("histogram");
            break;
        case METRIC_TYPE_GAUGE_HISTOGRAM:
            sb.append("gauge_histogram");
            break;
        }

        if (!this._metrics.isEmpty()) {
            sb.append(",metrics: ");
            int i = 0;
            for (Metric metric : this._metrics) {
                if (i != 0)
                    sb.append(",");
                sb.append("(").append(metric.toString()).append(")");
                i++;
            }
        }

        return sb.toString();
    }
}
