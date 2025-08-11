NCOLLECTD-DF(5) - File Formats Manual

# NAME

**ncollectd-df** - Documentation of ncollectd's df plugin

# SYNOPSIS

	load-plugin df
	plugin df {
	    device [incl|include|excl|exclude] device
	    mount-point [incl|include|excl|exclude] mount-point
	    fs-type [incl|include|excl|exclude] fs-type
	    log-once true|false
	}

# DESCRIPTION

The **df** plugin collects file system usage information, i. e. basically
how much space on a mounted partition is used and how much is available.
It's named after and very similar to the
df(1)
UNIX command that's been around forever.

The **df** plugin supports the following options:

**device** \[*incl|include|excl|exclude*] *device*

> Select partitions based on the devicename.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**mount-point** \[*incl|include|excl|exclude*] *mount-point*

> Select partitions based on the mountpoint.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**fs-type** \[*incl|include|excl|exclude*] *fs-type*

> Select partitions based on the filesystem type.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**log-once** *true|false*

> Only log
> **stat**()
> errors once.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
