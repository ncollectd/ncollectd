NCOLLECTD-CIFS(5) - File Formats Manual

# NAME

**ncollectd-cifs** - Documentation of ncollectd's cifs plugin

# SYNOPSIS

	load-plugin cifs
	plugin cifs {
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **cifs** plugin collectd statistics of mounted cifs filesystems.

The **cifs** plugin supports the following options:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

