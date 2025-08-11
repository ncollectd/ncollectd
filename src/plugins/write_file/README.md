NCOLLECTD-WRITE\_FILE(5) - File Formats Manual

# NAME

**ncollectd-write\_file** - Documentation of ncollectd's write\_file plugin

# SYNOPSIS

	load-plugin write_file
	plugin write_file {
	    instance name {
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	        format-notification text|json|protob
	        write metrics|notifications
	        file "file-name"
	    }
	}

# DESCRIPTION

The **write\_file** plugin writes the metrics to a file.

The plugin has the following options:

.Bl -tag -width Ds

> **format-metric** *influxdb|graphite|json|kairosdb|opentsdb|openmetrics|remote*
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

**format-notification** *text|json|protob*

> Selects the format in which notifications are written.

**write** *metrics|notifications*

> If set to *metrics* (the default) the plugin will handle metrics.
> If set to *notifications* the plugin will handle notifications.

**file** *file-name*

> If the filename is *stderr* the metrics are send to standard error,
> if the filename is *stdout* the metrics are send to standard output.
> If the option **file** is not set the metrics are send to the standard output.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
