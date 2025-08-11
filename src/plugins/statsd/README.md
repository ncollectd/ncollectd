NCOLLECTD-STATSD(5) - File Formats Manual

# NAME

**ncollectd-statsd** - Documentation of ncollectd's statsd plugin

# SYNOPSIS

	load-plugin statsd
	plugin statsd {
	    instance name {
	        host host
	        port port
	        delete-counters true|false
	        delete-timers true|false
	        delete-gauges true|false
	        delete-sets true|false
	        timer-buckets bucket [[bucket] ...]
	        interval seconds
	        metric-prefix prefix
	        label key value
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **statsd** plugin listens to a UDP socket, reads "events" in the statsd
protocol and dispatches rates or other aggregates of these numbers periodically.

The plugin implements the *counter*, *timer*, *gauge* and *set*
statds metrics.

The configuration of the **statsd** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
The following configuration options are valid:

**host** *host*

> Bind to the hostname / address *host*.
> By default, the plugin will bind to the "any" address, i.e. accept packets sent
> to any of the hosts addresses.

**port** *port*

> UDP port to listen to.
> This can be either a service name or a port number.
> Defaults to 8125.

**delete-counters** *true|false*

**delete-timers** *true|false*

**delete-gauges** *true|false*

**delete-sets** *true|false*

> These options control what happens if metrics are not updated in an interval.
> If set to *false*, the default, metrics are dispatched unchanged, i.e. the
> rate of counters and size of sets will be zero, timers report NaN
> and gauges are unchanged.
> If set to **true**, the such metrics are not dispatched and removed from
> the internal cache.

**timer-buckets** *bucket* \[\[*bucket*] ...]

> Config the buckets for the timer histograms.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**metric-prefix** *prefix*

> Prefix the metrics name with this strings.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
