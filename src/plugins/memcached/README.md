NCOLLECTD-MEMCACHED(5) - File Formats Manual

# NAME

**ncollectd-memcached** - Documentation of ncollectd's memcached plugin

# SYNOPSIS

	load-plugin memcached
	plugin memcached {
	    instance name {
	        socket /path/to/socket
	        host hostname
	        port port
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **memcached** plugin connects to a memcached server and queries
statistics about cache utilization, memory and bandwidth used.

The plugin configuration consists of one or more **instance** blocks which
specify one *memcached* connection each.
Within the **instance** blocks, the following options are allowed:

**socket** */path/to/socket*

> Connect to *memcached* using the UNIX domain socket
> at */path/to/socket*.
> If this setting is given, the **host** and **port** settings are ignored.

**host** *hostname*

> Hostname or IP to connect to.
> Defaults to **127.0.0.1**.

**port** *port*

> TCP port to connect to.
> Defaults to **11211**.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

