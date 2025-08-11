NCOLLECTD-PING(5) - File Formats Manual

# NAME

**ncollectd-ping** - Documentation of ncollectd's ping plugin

# SYNOPSIS

	load-plugin ping
	plugin ping {
	    instance name {
	        host ip-address
	        address-family [any|ipv4|ipv6]
	        source-address host
	        device name
	        ttl [0-255]
	        ping-interval seconds
	        interval seconds
	        size size
	        timeout seconds
	        max-missed packets
	        label key value
	        histogram-buckets [bucket] ...
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ping** plugin starts a new thread which sends ICMP "ping" packets to
the configured hosts periodically and measures the network latency.
Whenever the read function of the plugin is called, it submits the
average latency, the standard deviation and the drop rate for each host.

Available configuration options for a **instance** block:

**host** *ip-address*

> Host to ping periodically.
> This option may be repeated several times to ping multiple hosts.

**address-family** \[*any|ipv4|ipv6*]

> Sets the address family to use. **address-family** may be *any*,
> *ipv4* or *ipv6*.
> This option will be ignored if you set a **source-address**.

**source-address** *host*

> Sets the source address to use.
> *host* may either be a numerical network address or a network hostname.

**device** *name*

> Sets the outgoing network device to be used.
> *name* has to specify an interface name.
> This might not be supported by all operating systems.

**ttl** \[*0-255*]

> Sets the Time-To-Live of generated ICMP packets.

**ping-interval** *seconds*

> Sets the interval in which to send ICMP echo packets to the configured hosts.
> This is **not** the interval in which metrics are read from the plugin but the
> interval in which the hosts are "pinged".
> Therefore, the setting here should be smaller than or equal to the global
> **interval** setting.
> Fractional times, such as "1.24" are allowed.
> Default to **1.0**.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instance.

**size** *size*

> Sets the size of the data payload in ICMP packet to specified *size* (it
> will be filled with regular ASCII pattern).
> If not set, default 56 byte long string is used so that the packet size of an
> ICMPv4 packet is exactly 64 bytes, similar to the behaviour of normal
> ping(1)
> command.

**timeout** *seconds*

> Time to wait for a response from the host to which an ICMP packet had been
> sent.
> If a reply was not received after *second* seconds, the host is assumed
> to be down or the packet to be dropped.
> This setting must be smaller than the **interval** setting above for the
> plugin to work correctly.
> Fractional arguments are accepted.
> Default to **0.9**.

**max-missed** *packets*

> Trigger a DNS resolve after the host has not replied to *packets* packets.
> This enables the use of dynamic DNS services (like dyndns.org) with the
> ping plugin.
> Default to **-1** (disabled).

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**histogram-buckets** \[*bucket*] ...

> Define the list of buckets for the ICMP response time histogram.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
