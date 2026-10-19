NCOLLECTD-UUID(5) - File Formats Manual

# NAME

**ncollectd-uuid** - Documentation of ncollectd's uuid plugin

# SYNOPSIS

	load-plugin uuid
	plugin uuid {
	    uuid-file path
	}

# DESCRIPTION

The **uuid** plugin if loaded, causes the Hostname to be taken from
the machine's UUID.
The UUID is a universally unique designation for the machine, usually
taken from the machine's BIOS.
This is most useful if the machine is running in a virtual environment
such as Xen, in which case the UUID is preserved across shutdowns and
migration.

The following methods are used to find the machine's UUID, in order:

*	Check /etc/uuid (or **uuid-file**).

*	In **Darwin** get from system information name kern.uuid.

*	In **FreeBSD** get from system information name kern.hostuuid.

*	In **NetBSD** get from system information name
	machdep.dmi.system-uuid.

*	In **OpenBSD** get from system information level CTL\_HW
	and second level HW\_UUID.

*	In **Linux** from the files /sys/class/dmi/id/product\_uuid or
	/sys/hypervisor/uuid.

*	If no UUID can be found then the hostname is not modified.

The plugin has de following options:

**uuid-file** *path*

> Take the UUID from the given file (default */etc/uuid*).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

