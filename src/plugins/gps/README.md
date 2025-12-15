NCOLLECTD-GPS(5) - File Formats Manual

# NAME

**ncollectd-gps** - Documentation of ncollectd's gps plugin

# SYNOPSIS

	load-plugin gps
	plugin gps {
	    instance instance {
	        host hostname
	        port port
	        timeout seconds
	        pause-connect seconds
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **gps** plugin connects to gpsd on the host machine.
This is useful if you run an NTP server using a GPS for source and you
want to monitor it.
Mind your GPS must send $--GSA for having the data reported!

The following elements are collected:

**satellites**

> Number of satellites used for fix (type instance "used") and in view (type
> instance "visible"). 0 means no GPS satellites are visible.

**dilution\_of\_precision**

> Vertical and horizontal dilution (type instance "horizontal" or "vertical").
> It should be between 0 and 3.
> Look at the documentation of your GPS to know more.

> The available configuration options in the block **instance** are:

**host** *hostname*

> The host on which gpsd daemon runs.
> Defaults to **localhost**.

**port** *port*

> Port to connect to gpsd on the host machine.
> Defaults to **2947**.

**timeout** *seconds*

> Timeout in seconds (default 0.015 sec).
> The GPS data stream is fetch by the plugin form the daemon.
> It waits for data to be available, if none arrives it times out
> and loop for another reading.
> Mind to put a low value gpsd expects value in the micro-seconds area
> (recommended is 500 us) since the waiting function is blocking.
> Value must be between 500 us and 5 sec., if outside that range the
> default value is applied.
> This only applies from gpsd release-2.95.

**pause-connect** *seconds*

> Pause to apply between attempts of connection to gpsd in seconds
> (default 5 sec).

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

