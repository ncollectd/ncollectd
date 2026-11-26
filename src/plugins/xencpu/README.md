NCOLLECTD-XENCPU(5) - File Formats Manual

# NAME

**ncollectd-xencpu** - Documentation of ncollectd's xencpu plugin

# SYNOPSIS

	load-plugin xencpu
	plugin xencpu

# DESCRIPTION

The **xencpu** plugin collects metrics of hardware CPU load for machine
running Xen hypervisor.
The 'idle time' value provided by Xen is reported for each CPU (core)
as the counter **xen\_cpu\_idle\_seconds**.

This plugin doesn't have any options.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

