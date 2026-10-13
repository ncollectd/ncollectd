NCOLLECTD-FSCACHE(5) - File Formats Manual

# NAME

**ncollectd-fscache** - Documentation of ncollectd's fscache plugin

# SYNOPSIS

	load-plugin fscache
	plugin fscache {
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **fscache** plugin collects information about the file-system based
caching infrastructure for network file-systems and other slow media.
The cache was introduced into the Linux kernel in version 2.6.30.

The **fscache** plugin supports the following options:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

