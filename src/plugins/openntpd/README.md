NCOLLECTD-OPENNTPD(5) - File Formats Manual

# NAME

**ncollectd-openntpd** - Documentation of ncollectd's openntpd plugin

# SYNOPSIS

	load-plugin openntpd
	plugin openntpd {
	    instance instance {
	        socket-path path
	        timeout seconds
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **openntpd** plugin collects per-peer ntp data such as time offset and time
dispersion.

For talking to **openntpd**, it mimics what the **ntpdctl** control program does.

The configuration of the **openntpd** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.

The following options are accepted within each **instance** block:

**socket-path** *path*

> Path to the ntpd.sock file. The default value is /var/run/ntpd.sock.

**timeout** *seconds*

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

