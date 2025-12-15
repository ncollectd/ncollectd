NCOLLECTD-XENCPU(5) - File Formats Manual

# NAME

**ncollectd-xencpu** - Documentation of ncollectd's xencpu plugin

# SYNOPSIS

	load-plugin xencpu
	plugin xencpu

# DESCRIPTION

The **xencpu** plugin collects metrics of hardware CPU load for machine
running Xen hypervisor.
Load is calculated from 'idle time' value, provided by Xen.
Result is reported using the C&lt;percent&gt; type, for each CPU (core).

This plugin doesn't have any options.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

