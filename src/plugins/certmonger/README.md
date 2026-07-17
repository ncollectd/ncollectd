NCOLLECTD-CERTMONGER(5) - File Formats Manual

# NAME

**ncollectd-certmonger** - Documentation of ncollectd's certmonger plugin

# SYNOPSIS

	load-plugin certmonger
	plugin certmonger {
	    request [incl|include|excl|exclude] name
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **certmonger** plugin collects information of certificates tracked
by
certmonger(8).

The following configuration options are available:

**request** \[*incl|include|excl|exclude*] *name*

> Select the request *name*.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

