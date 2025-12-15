NCOLLECTD-GPU\_INTEL(5) - File Formats Manual

# NAME

**ncollectd-gpu\_intel** - Documentation of ncollectd's gpu\_intel plugin

# SYNOPSIS

	load-plugin gpu_intel
	plugin gpu_intel {
	    collect flags
	    log-gpu-info true|false
	    metrics-output string
	    samples samples
	}

# DESCRIPTION

This plugin is available on Linux only.
It uses level-Zero Sysman API to read GPU information.

Options below give overview of what information it could provide, but
the information actually available through it depends on what is
supported by the underlying GPU HW, its kernel driver, and Level-Zero
Sysman backend (compute) driver implementation.

**collect** *flags*

> **engine**

> > Engine utilization metrics collection.

> **engine\_single**

> > Utilization metrics collection for single engines i.e. provide
> > utilization information only for engine groups.

> **fabric**

> > Fabric port metrics collection.

> **frequency**

> > Actual / requested frequency metrics collection.

> **memory**

> > Memory usage metrics collection.

> **memory\_bandwidth**

> > Memory bandwidth metrics collection.

> **power**

> > Power usage metrics collection.

> **power\_ratio**

> **errors**

> > RAS (Reliability, Availability, and Serviceability) error
> > metrics collection.

> **separate\_errors**

> > Each error sub-category being reported separately, and just
> > report total error counters for higher level "correctable" and
> > "uncorrectable" errors.

> **temperature**

> > Temperature metrics collection.

> **throttle\_time**

> > Frequency throttling time metric collection.

**log-gpu-info** *true|false*

> If enabled, plugin logs at start some information about plugin
> settings, all the GPUs detected through Sysman API, and enables
> "pci\_dev" PCI device ID label for the metrics.

**metrics-output** *string*

> Set of "base", "rate", and "ratio" strings, separated by comma, colon,
> slash or space.

> Base metric can be either a counter (e.g. error count) that only
> increases, or one that can also decrease (e.g. temperature).
> The other options are values derived from base metric value.

> Several of the metric types support multiple output variants for their values.
> This option specifies which ones of them are to be reported.

> Default is to report all variants ("base:rate:ratio"). To reduce
> amount of data, it is better to configure just the relevant ones for
> given use (e.g. "base:ratio" or "rate:ratio").
> Note that some of the metric types support only two of these variants,
> whereas metrics supporting only single variant ignore this option.

> Base metric variant (e.g. HW energy usage as Joules counter) is
> preferred by Prometheus as doing rate calculations in Prometheus is
> more flexible.
> However, because collectd stores counters internally as integers instead
> of floating point, counter metrics are given in microjoules / microseconds
> instead of their (joule / second) SI base units
> (required by OpenMetrics spec), for better accuracy.

> Rate metric variant is directly human readable, and available for
> metrics where it makes sense (e.g. bytes per second, Watts and RPMs).

> Ratio variant is a utilization metric.
> It can be reported only for metric types which are either based on time
> (e.g. GPU engine use time), or for which Sysman provides a
> limit / maximum value.
> Some metrics may give over 100% ratios if their limit applies over longer
> time than the query interval (could happen e.g. with power limits).

**samples** *samples*

> How many values to collect (at specified plugin Interval) for sampled
> metrics, before submitting their (potentially aggregate) metric
> values.
> This means that the actual GPU metrics submit interval is
> interval \* samples.
> Currently GPU frequency and GPU memory values are sampled (like this),
> because their values can have very large fluctuations multiple times
> per second.
> When Samples is larger than 1, min + max are calculated for the sampled
> values and submitted instead of the read values themselves.

> Most of the other metric values are either counters, or change much
> slower, and are read only at submit intervals.
> If collecting of the sampled values is disabled, it is better to
> set Samples to 1 (default).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

