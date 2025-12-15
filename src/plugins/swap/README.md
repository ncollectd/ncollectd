NCOLLECTD-SWAP(5) - File Formats Manual

# NAME

**ncollectd-swap** - Documentation of ncollectd's swap plugin

# SYNOPSIS

	load-plugin swap
	plugin swap {
	    report-by-device true|false
	}

# DESCRIPTION

The **swap** plugin collects information about used and available swap space.
On *Linux* and *Solaris*, the following options are available:

**report-by-device** *true|false*

> Configures how to report physical swap devices.
> If set to **false** (the default), the summary over all swap devices is
> reported only, i.e. the globally used and available space over all devices.
> If **true** is configured, the used and available space of each device
> will be reported separately.

> This option is only available if the **swap** plugin can read
> */proc/swaps* (under Linux) or use the
> swapctl(2)
> mechanism (under *Solaris*).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

