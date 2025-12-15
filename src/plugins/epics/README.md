NCOLLECTD-EPICS(5) - File Formats Manual

# NAME

**ncollectd-epics** - Documentation of ncollectd's epics plugin

# SYNOPSIS

	load-plugin epics
	plugin epics {
	    metric-prefix prefix
	    label key value
	    metric {
	        metric metric
	        help help
	        type [gauge | counter]
	        label key index
	        label-from key pv-name
	        value-from pv-name [index]
	    }
	}

# DESCRIPTION

The **epics** plugin collects data from EPICS (Experimental Physics and
Industrial Control System) message bus.

Note that in addition to the plugin configuration, the host OS has to be
configured to be part of the message bus: caRepeater daemon is up and running
and appropriate EPICS environment variables are set, if required.
Consult EPICS documentation for details.

The plugin configuration consists of multiple **metric** blocks,
each per monitored EPICS Process Variable (PV).
The variables are constantly monitored and their values are latched once per
plugin interval.

**metric-prefix** *prefix*

> Prepends *prefix* to the metrics name.

**label** *key* *value*

> Append the label *key*=*value* to all metrics.

**metric**

> The metric block defines a metric to collect from the EPICS message bus.

> **metric** *metric*

> > Set the metric name.

> **help** *help*

> > Set the **help** text for the metric.

> **type** \[*gauge* | *counter*]

> > The **type** that's used for each metric returned.
> > Must be *gauge* or *counter*,

> **label** *key* *index*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **metric** block.

> **label-from** *key* *pv-name*

> > String-typed PV can be monitored as a label.
> > For instance, this can potentially be useful for tracking an experiment ID.

> **value-from** *pv-name* \[*index*]

> > Especifies the PV to collect the value of the metric.
> > If the index not set is 0.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

