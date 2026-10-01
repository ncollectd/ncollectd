NCOLLECTD-ARPCACHE(5) - File Formats Manual

# NAME

**ncollectd-arpcache** - Documentation of ncollectd's arpcache plugin

# SYNOPSIS

	load-plugin arpcache
	plugin arpcache {
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **arpcache** plugin collect statistics around neighbor cache and ARP,
reading the files */proc/net/stat/arp\_cache* for IPv4 and
*/proc/net/stat/ndisc\_cache* for IPV6.

The **arpcache** plugin supports the following options:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

