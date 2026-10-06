NCOLLECTD-BCACHE(5) - File Formats Manual

# NAME

**ncollectd-bcache** - Documentation of ncollectd's bcache plugin

# SYNOPSIS

	load-plugin bcache
	plugin bcache {
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **bcache** plugin collect statistics from the block layer cache Bcache.
The **bcache** plugin supports the following options:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

