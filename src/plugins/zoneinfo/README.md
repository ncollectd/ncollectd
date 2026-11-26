NCOLLECTD-ZONEINFO(5) - File Formats Manual

# NAME

**ncollectd-zoneinfo** - Documentation of ncollectd's zoneinfo plugin

# SYNOPSIS

	load-plugin zoneinfo
	plugin zoneinfo

# DESCRIPTION

The **zoneinfo** plugin collectd information about memory zones reading
the file */proc/zoneinfo*.

The metrics have the *node* and *zone* labels.
Since Linux 4.8 some statistics are kept per NUMA node and are shown in the
*per-node stats* section of the first populated zone of each node.
These metrics only have the *node* label.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

