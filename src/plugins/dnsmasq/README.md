NCOLLECTD-DNSMASQ(5) - File Formats Manual

# NAME

**ncollectd-dnsmasq** - Documentation of ncollectd's dnsmasq plugin

# SYNOPSIS

	load-plugin dnsmasq
	plugin dnsmasq {
	    instance instance {
	        host hostname
	        port port
	        label key value
	        timeout seconds
	        leases /path/to/leases
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **dnsmasq** plugin collect statistics of dnsmasq daemon.

The plugin configuration consists of one or more **instance** blocks which
specify one *dnsmasq* server each.
Within the **instance** blocks, the following options are allowed:

**host** *hostname*

> Hostname or address to connect to.
> Default to 127.0.0.1

**port** *port*

> Service name or port number to connect to.
> Defaults to 43

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**timeout** *seconds*

> Set the timeout for the DNS query to get metrics from dnsmasq.
> TODO

**leases** */path/to/leases*

> Leases file path.
> Default to */var/lib/misc/dnsmasq.leases*.
> TODO

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

ncollectd - - -
