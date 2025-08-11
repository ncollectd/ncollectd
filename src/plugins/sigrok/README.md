NCOLLECTD-SIGROK(5) - File Formats Manual

# NAME

**ncollectd-sigrok** - Documentation of ncollectd's sigrok plugin

# SYNOPSIS

	load-plugin sigrok
	plugin sigrok {
	    log-level level
	    device name {
	        driver driver
	        conn connection-spec
	        serial-comm serial-device
	        minimum-interval seconds
	        metric-prefix prefix
	        channel channel
	        label key value
	    }
	}

# DESCRIPTION

The **sigrok** plugin uses *libsigrok* to retrieve measurements
from any device supported by the sigrok project.

The plugin supports the following options:

**log-level** *level*

> The *sigrok* logging level to pass on to the ncollectd log, as a number
> between **0** and **5** (inclusive). These levels correspond to *None*,
> *Errors*, *Warnings*, *Informational*, *Debug* and *Spew*,
> respectively.
> The default is **2** (*Warnings*). The *sigrok* log messages,
> regardless of their level, are always submitted to ncollectd at its
> INFO log level.

**device** *name*

> A sigrok-supported device, uniquely identified by this section's options.
> The *name* is passed to ncollectd as the *device* label.

> **driver** *driver*

> > The sigrok driver to use for this device.

> **conn** *connection-spec*

> > If the device cannot be auto-discovered, or more than one might be discovered
> > by the driver, *connection-spec* specifies the connection string to the
> > device.
> > It can be of the form of a device path (e.g. */dev/ttyUSB2*), or, in
> > case of a non-serial USB-connected device, the USB
> > *VendorID*&zwnj;**.**&zwnj;*ProductID* separated by a period
> > (e.g. 0403.6001).
> > A USB device can also be specified as *Bus*&zwnj;**.**&zwnj;*Address*
> > (e.g. 1.41).

> **serial-comm** *serial-device*

> > For serial devices with non-standard port settings, this option can be used
> > to specify them in a form understood by *sigrok*, e.g. 9600/8n1.
> > This should not be necessary; drivers know how to communicate with devices they
> > support.

> **minimum-interval** *seconds*

> > Specifies the minimum time between measurement dispatches to ncollectd, in
> > seconds.
> > Since some *sigrok* supported devices can acquire measurements many
> > times per second, it may be necessary to throttle these.

> > The default **minimum-interval** is **0**, meaning measurements received
> > from the device are always dispatched to ncollectd.
> > When throttled, unused measurements are discarded.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metric name.

> **channel** *channel*

> > Send only metrics for this *channel* name.
> > It can appear multiple times.
> > By default, it shows all channels if not set.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times in the **device** block.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
