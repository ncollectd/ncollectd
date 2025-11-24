NCOLLECTD-NTPD(5) - File Formats Manual

# NAME

**ncollectd-ntpd** - Documentation of ncollectd's ntpd plugin

# SYNOPSIS

	load-plugin ntpd
	plugin ntpd {
	    instance instance {
	        host host
	        port port
	        timeout seconds
	        interval seconds
	        label key value
	        collect flags ...
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ntpd** plugin collects per-peer ntp data such as time offset and time
dispersion.

For talking to **ntpd**, it mimics what the **ntpq** control program does
on the wire - using **mode 6** specific requests.

The configuration of the **ntpd** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.

The following options are accepted within each **instance** block:

**host** *host*

> Hostname of the host running **ntpd**.
> Defaults to **localhost**.

**port** *port*

> UDP port to connect to.
> Defaults to **123**.

**timeout** *seconds*

> The **timeout** option sets the timeout for the requests to the the ntpd daemon.
> The default value is 2 seconds for the first package.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this instance.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**collect** *flags* ...

> **sysinfo**

> > Collect the set of metrics such as those from the command: ntpq -c sysinfo.

> **kerninfo**

> > Collect the set of metrics such as those from the command: ntpq -c kerninfo.

> **sysstats**

> > Collect the set of metrics such as those from the command: ntpq -c sysstats.

> **authinfo**

> > Collect the set of metrics such as those from the command: ntpq -c authinfo.

> **iostats**

> > Collect the set of metrics such as those from the command: ntpq -c iostats.

> **ntsinfo**

> > Collect the set of metrics such as those from the command: ntpq -c ntsinfo.

> **ntskeinfo**

> > Collect the set of metrics such as those from the command: ntpq -c ntskeinfo.

> **peers**

> > Collect peers metrics such as those from the command: ntpq -c peers

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

