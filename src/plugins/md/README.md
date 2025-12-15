NCOLLECTD-MD(5) - File Formats Manual

# NAME

**ncollectd-md** - Documentation of ncollectd's md plugin

# SYNOPSIS

	load-plugin md
	plugin md {
	    device [incl|include|excl|exclude] device-name
	}

# DESCRIPTION

The **md** plugin reports the number of disks in various states in Linux
software RAID devices.

The plugin support the following option:

**device** \[*incl|include|excl|exclude*] *device-name*

> Select md devices based on device name.
> The *device-name* is the basename of the device, i. e. the name of
> the block device without the leading /dev/.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

