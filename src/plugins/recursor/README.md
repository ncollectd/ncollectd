NCOLLECTD-RECURSOR(5) - File Formats Manual

# NAME

**ncollectd-recursor** - Documentation of ncollectd's recursor plugin

# SYNOPSIS

	load-plugin recursor
	plugin recursor {
	    instance name {
	        socket "path-to-socket"
	        local-socket "path-to-socket"
	        timeout seconds
	        label "key" "value"
	        interval seconds
	        protocol 1|2|3
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **recursor** plugin queries statistics from a PowerDNS recursor.

The configuration of the **recursor** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
The following options are accepted within each block:

**socket** *path-to-socket*

> Configures the path to the UNIX domain socket to be used when connecting to the
> daemon.
> By default /run/pdns-recursor/pdns\_recursor.controlsocket will be used.

**local-socket** *path-to-socket*

> Querying the recursor is done using UDP.
> When using UDP over UNIX domain sockets, the client socket needs a name
> in the file system, too.
> This is used for PowerDNS Recursor versions less than 4.6.0.
> The default is /var/run/ncollectd-powerdns.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for the requests.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.

**protocol** *1|2|3*

> The PowerDNS Recursor has diferents protocols to query depending on the version:

> **1**

> > For PowerDNS Recursor versions less than 4.5.0.
> > This is the default option.

> **2**

> > For PowerDNS Recursor versions between 4.5.0 and 4.5.11.

> **3**

> > For PowerDNS Recursor versions great than 4.6.0.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
