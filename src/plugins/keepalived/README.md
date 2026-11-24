NCOLLECTD-KEEPALIVED(5) - File Formats Manual

# NAME

**ncollectd-keepalived** - Documentation of ncollectd's keepalived plugin

# SYNOPSIS

	load-plugin keepalived
	plugin keepalived {
	    instance instance {
	        pid-path /path/to/pid
	        stats-path /path/to/stats
	        data-path /path/to/data
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **keepalived** plugin collect metrics from a keepalived process.

The following options are accepted within each **instance** block:

**pid-path** */path/to/pid*

> Use the specified pidfile for the keepalived.
> Default to */var/run/keepalived.pid*.

**stats-path** */path/to/stats*

> Default to */tmp/keepalived.stats*
> The file must be a regular file owned by root, symbolic links are not followed.
> It is recommended to configure keepalived to write it outside */tmp*.

**data-path** */path/to/data*

> Default to */tmp/keepalived.data*
> The file must be a regular file owned by root, symbolic links are not followed.
> It is recommended to configure keepalived to write it outside */tmp*.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

