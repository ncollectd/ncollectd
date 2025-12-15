NCOLLECTD-WRITE\_UDP(5) - File Formats Manual

# NAME

**ncollectd-write\_udp** - Documentation of ncollectd's write\_udp plugin

# SYNOPSIS

	load-plugin write_udp
	plugin write_udp {
	    instance name {
	        host host
	        port port
	        ttl ttl
	        packet-max-size size
	        flush-interval seconds
	        flush-timeout seconds
	        format-metric influxdb|graphite|kairosdb|opentsdb
	    }
	}

# DESCRIPTION

The **write\_udp** plugin send the metrics as UDP packets.

The following options are accepted within each **instance** block:

**host** *host*

> Hostname or address to connect to.
> Defaults to localhost.

**port** *port*

> Service name or port number to connect to.

**ttl** *ttl*

**packet-max-size** *size*

**flush-interval** *seconds*

**flush-timeout** *seconds*

**format-metric** *influxdb|graphite|kairosdb|opentsdb*

> Selects the format in which metrics are written.

> **influxdb** \[*sec|msec|usec|nsec*]

> > Format the metrics with the influxdb line format.

> **graphite**

> > Format the metrics with the graphite plaintext format.

> **kairosdb** \[*sec|msec*]

> > Format the metrics with the karirosdb text format.

> **opentsdb**

> > Format the metrics with the opentsdb text format.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

