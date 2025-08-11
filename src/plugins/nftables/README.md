NCOLLECTD-NFTABLES(5) - File Formats Manual

# NAME

**ncollectd-nftables** - Documentation of ncollectd's nftables plugin

# SYNOPSIS

	load-plugin nftables
	plugin nftables {
	    counter  ip|ip6|arp|birdge|netdev|inet [table] [counter]
	}

The **nftables** plugin collects information about the packet and byte count
of named counters inside your ruleset.

The plugin has the following options:

**counter** *ip|ip6|arp|birdge|netdev|inet* \[*table*] \[*counter*]

> If only the *family* is specified all named counters on the family tables are collected.
> If *family* and *table* are specified, only the counters for this table are collectd.
> You can optionally specify the *counter* name to collect a specific counter.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
