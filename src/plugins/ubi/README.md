NCOLLECTD-UBI(5) - File Formats Manual

# NAME

**ncollectd-ubi** - Documentation of ncollectd's ubi plugin

# SYNOPSIS

	load-plugin ubi
	plugin ubi {
	    device [incl|include|excl|exclude] name
	}

# DESCRIPTION

The **ubi** plugin collects some statistics about the
UBI (Unsorted Block Image).
Values collected are the number of bad physical eraseblocks on
the underlying MTD (Memory Technology Device) and the maximum erase
counter value concerning one volume.

See following links for details:
[http://www.linux-mtd.infradead.org/doc/ubi.html](http://www.linux-mtd.infradead.org/doc/ubi.html)
[http://www.linux-mtd.infradead.org/doc/ubifs.html](http://www.linux-mtd.infradead.org/doc/ubifs.html)
[https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-ubi](https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-ubi)

The plugin has the following options:

**device** \[*incl|include|excl|exclude*] *name*

> Select the device *name* of the UBI volume.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
