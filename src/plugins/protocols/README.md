NCOLLECTD-PROTOCOLS(5) - File Formats Manual

# NAME

**ncollectd-protocols** - Documentation of ncollectd's protocols plugin

# SYNOPSIS

	load-plugin protocols
	plugin protocols {
	    collect [ip|icmp|udp|udplite|udplite6|ip6|icmp6|udp6|tcp|mptcp|sctp]
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **protocols** plugin collects a lot of information about various
network protocols, such as *IP*, *TCP*, *UDP*, etc.

Available configuration options:

**collect** *\[ip|icmp|udp|udplite|udplite6|ip6|icmp6|udp6|tcp|mptcp|sctp]*

> **ip**

> **icmp**

> **udp**

> **udplite**

> **udplite6**

> **ip6**

> **icmp6**

> **udp6**

> **tcp**

> **mptcp**

> **sctp**

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
