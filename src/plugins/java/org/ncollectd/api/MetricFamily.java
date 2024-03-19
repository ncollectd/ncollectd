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
            _type = type;
            break;
        default:
            _type = METRIC_TYPE_UNKNOWN;
            break;
        }
        _name = name;
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
            _type = type;
            break;
        default:
            _type = METRIC_TYPE_UNKNOWN;
            break;
        }
        _name = name;
        _help = help;
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
            _type = type;
            break;
        default:
            _type = METRIC_TYPE_UNKNOWN;
            break;
        }
        _name = name;
        _help = help;
        _unit = unit;
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
            _type = type;
            break;
        default:
            _type = METRIC_TYPE_UNKNOWN;
            break;
        }
        _name = name;
        _help = help;
        _unit = unit;
        _metrics = metrics;
    }

    public String getName()
    {
        return _name;
    }

    public void setName(String name)
    {
        _name = name;
    }

    public String getHelp()
    {
        return _help;
    }

    public void setHelp(String help)
    {
        _help = help;
    }

    public String getUnit()
    {
        return _unit;
    }

    public void setUnit(String unit)
    {
        _unit = unit;
    }

    public List<Metric> getMetrics()
    {
        return _metrics;
    }

    public void setMetrics(List<Metric> metrics)
    {
        _metrics = metrics;
    }

    public void addMetric(Metric metric)
    {
        _metrics.add(metric);
    }

    public void clearMetrics()
    {
        _metrics.clear();
    }

    public String toString()
    {
        StringBuffer sb = new StringBuffer();
/*
        sb.append('[').append(new Date(_time)).append("] ");
        sb.append(getSource());
*/
        return sb.toString();

    }
}
