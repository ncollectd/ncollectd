NCOLLECTD-WIRELESS(5) - File Formats Manual

# NAME

**ncollectd-wireless** - Documentation of ncollectd's wireless plugin

# SYNOPSIS

	load-plugin wireless
	plugin wireless {
	    device [incl|include|excl|exclude] name
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **wireless** plugin collectd statistics of the signal
in wireless interfaces.
The plugin has the following options:

**device** \[*incl|include|excl|exclude*] *name*

> Select the wireless device *name*.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

