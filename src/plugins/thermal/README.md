NCOLLECTD-THERMAL(5) - File Formats Manual

# NAME

**ncollectd-thermal** - Documentation of ncollectd's thermal plugin

# SYNOPSIS

	load-plugin thermal
	plugin thermal {
	    device  [incl|include|excl|exclude] device
	    force-use-procfs true|false
	}

# DESCRIPTION

The **thermal** plugin reads ACPI thermal zone information from the sysfs or
procfs file system, i. e. mostly system temperature information.

**device** \[*incl|include|excl|exclude*] *device*

> Selects the name of the thermal device that you want to collect or ignore.
> This option may be used multiple times to specify a list of devices.
> If no selection is configured at all, **all** devices are selected.

**force-use-procfs** *true|false*

> By default, the *thermal* plugin tries to read the statistics from the Linux
> *sysfs* interface.
> If that is not available, the plugin falls back to the *procfs* interface.
> By setting this option to *true*, you can force the plugin to use
> the latter.
> This option defaults to *false*.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
