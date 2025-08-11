NCOLLECTD-NTPD(5) - File Formats Manual

# NAME

**ncollectd-ntpd** - Documentation of ncollectd's ntpd plugin

# SYNOPSIS

	load-plugin ntpd
	plugin ntpd {
	    instance instance {
	        host host
	        port port
	        label key value
	        interval seconds
	        reverse-lookups true|false
	        include-unit-id true|false
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ntpd** plugin collects per-peer ntp data such as time offset and time
dispersion.

For talking to **ntpd**, it mimics what the **ntpdc** control program does
on the wire - using **mode 7** specific requests.
This mode is deprecated with newer **ntpd** releases (4.2.7p230 and later).
For the **ntpd** plugin to work correctly with them, the ntp daemon must be
explicitly configured to enable **mode 7** (which is disabled by default).
Refer to the
ntp.conf(5)
manual page for details.

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

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this instance.

**reverse-lookups** *true|false*

> Sets whether or not to perform reverse lookups on peers.
> Since the name or IP-address may be used in a filename it is recommended
> to disable reverse lookups.
> The default is to do reverse lookups to preserve backwards compatibility,
> though.

**include-unit-id** *true|false*

> When a peer is a refclock, include the unit ID in the *peer* label.
> Defaults to **false**.
> If two refclock peers use the same driver and this is **false**, the plugin
> will try to write simultaneous measurements from both to the same type instance.
> This will result in error messages in the log and only one set of measurements
> making it through.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
