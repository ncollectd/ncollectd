NCOLLECTD-SLABINFO(5) - File Formats Manual

# NAME

**ncollectd-slabinfo** - Documentation of ncollectd's slabinfo plugin

# SYNOPSIS

	load-plugin slabinfo
	plugin slabinfo {
	    cache [incl|include|excl|exclude] cache
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **slabinfo** plugin collect information about kernel caches reading the
file /proc/slabinfo.

The **slabinfo** plugin supports the following options:

**cache** \[*incl|include|excl|exclude*] *cache*

> Select kernel cache based on the cache name.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

