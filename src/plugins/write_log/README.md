NCOLLECTD-WRITE\_LOG(5) - File Formats Manual

# NAME

**ncollectd-write\_log** - Documentation of ncollectd's write\_log plugin

# SYNOPSIS

	load-plugin write_log
	plugin write_log {
	    format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	}

# DESCRIPTION

The **write\_log plugin** plugin writes metrics as INFO log messages.

The plugin has the following options:

**format-metric** *influxdb|graphite|json|kairosdb|opentsdb|openmetrics|remote*

> Selects the format in which metrics are written.

> **influxdb** \[*sec|msec|usec|nsec*]

> > Format the metrics with the influxdb line format.

> **graphite**

> > Format the metrics with the graphite plaintext format.

> **json**

> > Format the metrics as json message.

> **kairosdb** \[*telnet \[sec|msec]|json*]

> > Format the metrics with the karirosdb text format or json format.

> **opentsdb** \[telnet|json]

> > Format the metrics with the opentsdb text format or json format.

> **openmetrics** \[text|protob]

> > Format the metrics with the openmetrics text format.

> **opentelemetry** \[json]

> > Format the metrics with the opentelemetry json format.

> **remote** \[metadata]

> > Format the metrics with the protocol buffer remote format.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
