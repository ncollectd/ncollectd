NCOLLECTD-ZRAM(5) - File Formats Manual

# NAME

**ncollectd-zram** - Documentation of ncollectd's zram plugin

# SYNOPSIS

	load-plugin zram
	plugin zram {
	    device [incl|include|excl|exclude] name
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **zram** plugin report metrics from the compressed block device
in RAM (zram).

Using the following options you can ignore some zram devices or configure the
collection only of specific zram devices.

**device** \[*incl|include|excl|exclude*] *name*

> Select the zram device *name*.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

