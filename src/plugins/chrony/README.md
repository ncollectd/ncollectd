NCOLLECTD-CHRONY(5) - File Formats Manual

# NAME

**ncollectd-chrony** - Documentation of ncollectd's chrony plugin

# SYNOPSIS

	load-plugin chrony
	plugin chrony {
	    instance instance {
	        host hostname
	        port port
	        timeout seconds
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **chrony** plugin collects ntp data from a **chronyd** server,
such as clock skew and per-peer stratum.

For talking to **chronyd**, it mimics what the **chronyc** control
program does on the wire.

The configuration of the **chrony** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.

The following options are accepted within each **instance** block:

**host** *hostname*

> Hostname of the host running **chronyd**.
> Defaults to **localhost**.

**port** *port*

> UDP port to connect to.
> Defaults to **323**.

**timeout** *seconds*

> Connection timeout in seconds.
> Defaults to **2**.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this server.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
