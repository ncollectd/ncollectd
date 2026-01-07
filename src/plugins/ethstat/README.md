NCOLLECTD-ETHSTAT(5) - File Formats Manual

# NAME

**ncollectd-ethstat** - Documentation of ncollectd's ethstat plugin

# SYNOPSIS

	load-plugin ethstat
	plugin ethstat {
	    interface name {
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ethstat** plugin collects information about network interface cards
(NICs) by talking directly with the underlying kernel driver using
ioctl(2).

The plugin supports the following options:

**interface** *name*

> Collect statistical information about interface *name*.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

