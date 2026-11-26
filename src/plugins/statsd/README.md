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
	        max-metrics number
	        max-set-size number
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
Timers are reported as cumulative histograms: like counters, they are not
reset between intervals.
Values that are not finite numbers, negative counter increments and
negative timer values are discarded.

The statsd protocol has no authentication: anyone who can send packets to the
listening port can create new metrics and use memory of the daemon.
Restrict the access to the port with a firewall and consider setting the
**max-metrics** and **max-set-size** limits.

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
> rate of counters and timers and the size of sets will be zero
> and gauges are unchanged.
> If set to **true**, the such metrics are not dispatched and removed from
> the internal cache.

**timer-buckets** *bucket* \[\[*bucket*] ...]

> Config the buckets for the timer histograms.
> Timer values are received in milliseconds and stored in the histograms
> in seconds, so the bucket boundaries are in seconds.
> Defaults to 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5 and 10.

**max-metrics** *number*

> Maximum number of metrics (distinct combinations of type, name and tags)
> kept by this instance.
> When the limit is reached, lines for new metrics are dropped and the number
> of dropped lines is logged once per interval.
> Defaults to 0, no limit.

**max-set-size** *number*

> Maximum number of distinct values counted in each set during an interval.
> Values exceeding the limit are dropped and their number is logged once per
> interval.
> Defaults to 0, no limit.

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

