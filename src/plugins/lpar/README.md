NCOLLECTD-LPAR(5) - File Formats Manual

# NAME

**ncollectd-lpar** - Documentation of ncollectd's lpar plugin

# SYNOPSIS

	load-plugin lpar
	plugin lpar {
	    cpu-pool-stats true|false
	}

# DESCRIPTION

The **lpar** plugin reads CPU statistics of *Logical Partitions*, a
virtualization technique for IBM POWER processors.
It takes into account CPU time stolen from or donated to a partition,
in addition to the usual user, system, I/O statistics.

The following configuration options are available:

**cpu-pool-stats** *true|false*

> When enabled, statistics about the processor pool are read, too.
> The partition needs to have pool authority in order to be able to acquire
> this information.
> Defaults to false.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
