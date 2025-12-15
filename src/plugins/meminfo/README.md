NCOLLECTD-MEMINFO(5) - File Formats Manual

# NAME

**ncollectd-meminfo** - Documentation of ncollectd's meminfo plugin

# SYNOPSIS

	load-plugin meminfo
	plugin meminfo {
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **meminfo** plugin reports the metrics from the */proc/meminfo* file.

The following options are accepted:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

