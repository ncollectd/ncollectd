NCOLLECTD-INFINIBAND(5) - File Formats Manual

# NAME

**ncollectd-infiniband** - Documentation of ncollectd's infiniband plugin

# SYNOPSIS

	load-plugin infiniband
	plugin infiniband {
	    port [incl|include|excl|exclude] port
	}

# DESCRIPTION

The **infiniband** plugin collects information about IB ports.
Metrics are gathered from */sys/class/infiniband/DEVICE/port/PORTNUM/\*&zwnj;*,
and *port* names are formatted like DEVICE:PORTNUM.

The plugin support the following options:

**port** \[*incl|include|excl|exclude*] *port*

> Select the *port* to collectd data from.
> If no **port** option is configured, all ports are collected.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
