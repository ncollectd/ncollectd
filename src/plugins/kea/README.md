NCOLLECTD-KEA(5) - File Formats Manual

# NAME

**ncollectd-kea** - Documentation of ncollectd's kea plugin

# SYNOPSIS

	load-plugin kea
	plugin kea {
	    instance name {
	        socket-path "/path-to-socket"
	        timeout seconds
	        label "key" "value"
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **kea** plugin collect metris from Kea DHCP server.

In the **plugin** block, there may be one or more **instance** blocks,
each defining a Kea instance.

The following options are valid within **instance** blocks:

**socket-path** */path-to-socket*

> Path to control Unix socket to retrieve metrics.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for the requests to
> the **kea** server, in seconds.
> By default, the configured **interval** is used to set the timeout.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

