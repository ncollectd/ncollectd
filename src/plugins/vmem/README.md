NCOLLECTD-VMEM(5) - File Formats Manual

# NAME

**ncollectd-vmem** - Documentation of ncollectd's vmem plugin

# SYNOPSIS

	load-plugin vmem
	plugin vmem {
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **vmem** plugin collects information about the usage of virtual memory.

The following options are accepted:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

