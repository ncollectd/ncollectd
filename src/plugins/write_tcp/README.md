NCOLLECTD-WRITE\_TCP(5) - File Formats Manual

# NAME

**ncollectd-write\_tcp** - Documentation of ncollectd's write\_tcp plugin

# SYNOPSIS

	load-plugin write_tcp
	plugin write_tcp {
	    instance name {
	        host host
	        port port
	        resolve-interval seconds
	        resolve-jitter seconds
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	    }
	}

# DESCRIPTION

The **write\_tcp** plugin writes the metrics through a tcp connection.

The following options are accepted within each **instance** block:

**host** *host*

> Hostname or IP-address of the destination connection.

**port** *port*

> Service name or port number of the destination connection.

**resolve-interval** *seconds*

**resolve-jitter** *seconds*

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
