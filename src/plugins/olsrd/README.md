NCOLLECTD-OLSRD(5) - File Formats Manual

# NAME

**ncollectd-olsrd** - Documentation of ncollectd's olsrd plugin

# SYNOPSIS

	load-plugin olsrd
	plugin olsrd {
	    instance instance {
	        host hostname
	        port port
	        interval seconds
	        label key value
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **olsrd** plugin connects to the TCP port opened by
the *txtinfo* plugin of the Optimized Link State Routing daemon
and reads information about the current state of the meshed network.

The following configuration options are understood:

**host** *hostname*

> Connect to *host*.
> Defaults to **localhost**.

**port** *port*

> Specifies the port to connect to.
> This must be a string, even if you give the port as a number rather than
> a service name.
> Defaults to 2006.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
