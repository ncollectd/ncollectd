NCOLLECTD-IPTABLES(5) - File Formats Manual

# NAME

**ncollectd-iptables** - Documentation of ncollectd's iptables plugin

# SYNOPSIS

	load-plugin iptables
	plugin iptables {
	    chain fable chain [comment|number [name]]
	    chain6 fable chain [comment|number [name]]
	}

# DESCRIPTION

The **iptables** plugin can gather statistics from your ip\_tables based
packet filter (aka. firewall) for both the IPv4 and the IPv6 protocol.
It can collect the byte-counters and packet-counters of selected rules
and submit them to ncollectd.
You can select rules that should be collected either by their position
(e. g. &#8220;the fourth rule in the &#8216;INPUT&#8217; queue in the
&#8216;filter&#8217; table&#8221;) or by its comment (using the &#8220;COMMENT&#8221; match).
This means that depending on your firewall layout you can collect certain
services (such as the amount of web-traffic), source or destination hosts
or networks, dropped packets and much more.

The plugin support the following options:

**chain** *fable* *chain* \[*comment|number* \[*name*]]

**chain6** *fable* *chain* \[*comment|number* \[*name*]]

> Select the iptables/ip6tables filter rules to count packets and bytes from.

> If only *table* and *chain* are given, this plugin will collect
> the counters of all rules which have a comment-match.

> If *comment* or *number* is given, only the rule with the matching
> comment or the *n*th rule will be collected.
> Again, the comment (or the number) will be used as the type-instance.

> If *name* is supplied, it will be used as the type-instance instead of the
> comment or the number.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
