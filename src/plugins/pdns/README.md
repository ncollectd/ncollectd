NCOLLECTD-PDNS(5) - File Formats Manual

# NAME

**ncollectd-pdns** - Documentation of ncollectd's pdns plugin

# SYNOPSIS

	load-plugin pdns
	plugin pdns {
	    instance name {
	        socket "path-to-socket"
	        timeout seconds
	        label "key" "value"
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **pdns** plugin queries statistics from an authoritative PowerDNS
nameserver.

The configuration of the **pdns** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.

The **instance** block defines one authoritative server to query,
the following options are accepted within each block:

**socket** *path-to-socket*

> Configures the path to the UNIX domain socket to be used when connecting to the
> daemon.
> By default /run/pdns/pdns.controlsocket will be used.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for the requests.

**label** *key* *value*

>  Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

