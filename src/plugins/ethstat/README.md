NCOLLECTD-ETHSTAT(5) - File Formats Manual

# NAME

**ncollectd-ethstat** - Documentation of ncollectd's ethstat plugin

# SYNOPSIS

	load-plugin ethstat
	plugin ethstat {
	    interface name {
	        gauge [incl|include|excl|exclude] stat-name
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
> All the statistics are reported as counters unless selected with the
> **gauge** option.

> **gauge** \[*incl|include|excl|exclude*] *stat-name*

> > Report the statistics whose driver name match *stat-name* as gauges
> > instead of counters.
> > *stat-name* can be a string or a regular expression.
> > This option can be given multiple times.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

