NCOLLECTD-UNBOUND(5) - File Formats Manual

# NAME

**ncollectd-unbound** - Documentation of ncollectd's unbound plugin

# SYNOPSIS

	load-plugin unbound
	plugin unbound {
	    instance name {
	        host host
	        port port
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **unbound** plugin collect metrics from the Unbound resolver.

The configuration consists of one or more **instance** blocks:

**host** *host*

> Hostname to connect.

**port** *port*

> Port to connect.

**server-name** *servername*

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

