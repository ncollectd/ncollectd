NCOLLECTD-CONNTRACK(5) - File Formats Manual

# NAME

**ncollectd-conntrack** - Documentation of ncollectd's conntrack plugin

# SYNOPSIS

	load-plugin conntrack
	plugin conntrack

# DESCRIPTION

The **conntrack plugin** collects IP conntrack statistics reading the files:
*/proc/sys/net/netfilter/nf\_conntrack\_count* and
*/proc/sys/net/netfilter/nf\_conntrack\_max*.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
