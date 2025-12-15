NCOLLECTD-BUDDYINFO(5) - File Formats Manual

# NAME

**ncollectd-buddyinfo** - Documentation of ncollectd's buddyinfo plugin

# SYNOPSIS

	load-plugin buddyinfo
	plugin buddyinfo {
	    zone zone-name
	}

# DESCRIPTION

The **buddyinfo** plugin collects information by reading
*/proc/buddyinfo*.
This file contains information about the number of available contagious
physical pages at the moment.

The plugin has de following options:

**zone** *zone-name*

> Zone to collect info about.
> Will collect all zones by default.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

