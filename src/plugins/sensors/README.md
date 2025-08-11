NCOLLECTD-SENSORS(5) - File Formats Manual

# NAME

**ncollectd-sensors** - Documentation of ncollectd's sensors plugin

# SYNOPSIS

	load-plugin sensors
	plugin sensors {
	    sensor [incl|include|excl|exclude] sensor
	    sensor-config-file /path/to/config
	    use-labels true|false
	}

# DESCRIPTION

The **sensors** plugin uses *lm\_sensors* to retrieve sensor-values.
This means that all the needed modules have to be loaded and lm\_sensors
has to be configured (most likely by editing /etc/sensors.conf.
Read
sensors.conf(5)
for details.

**sensor** \[*incl|include|excl|exclude*] *sensor*

> Selects the name of the sensor which you want to collect or ignore
> For example, the option:

> >     sensor "it8712-isa-0290/voltage-in1"

> will cause collectd to gather data for the voltage sensor *in1* of
> the *it8712* on the isa bus at the address 0290.

> If no configuration if given, the **sensors** plugin will collect
> data from all sensors.
> This may not be practical, especially for uninteresting sensors.

**sensor-config-file** */path/to/config*

> Read the *lm\_sensors* configuration from */path/to/config*.
> When unset (recommended), the library's default will be used.

**use-labels** *true|false*

> Configures how sensor readings are reported.
> When set to **true**, sensor readings are reported using their descriptive
> label (e.g. "VCore").
> When set to **false** (the default) the sensor name is used ("in0").

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
