NCOLLECTD-SENSORS(5) - File Formats Manual

# NAME

**ncollectd-sensors** - Documentation of ncollectd's sensors plugin

# SYNOPSIS

	load-plugin sensors
	plugin sensors {
	    sensor-config-file /path/to/config
	    use-labels true|false
	}

# DESCRIPTION

The **sensors** plugin uses *lm\_sensors* in Linux systems
to retrieve sensor-values.
This means that all the needed modules have to be loaded and lm\_sensors
has to be configured (most likely by editing /etc/sensors.conf.
Read
sensors.conf(5)
for details.

**sensor-config-file** */path/to/config*

> Read the *lm\_sensors* configuration from */path/to/config*.
> When unset (recommended), the library's default will be used.
> Only for Linux systems.

**use-labels** *true|false*

> Configures how sensor readings are reported.
> When set to **true**, sensor readings are reported using their descriptive
> label (e.g. "VCore").
> When set to **false** (the default) the sensor name is used ("in0").
> Only for Linux systems.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

