NCOLLECTD-PCAP(5) - File Formats Manual

# NAME

**ncollectd-pcap** - Documentation of ncollectd's pcap plugin

# SYNOPSIS

	load-plugin pcap
	plugin pcap {
	    instance name {
	        interface interface
	        promiscuous true|false
	        ignore-source source
	        ignore-destination destination
	        filter expression
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **pcap** plugin uses libpcap to get a copy of all traffic on the
selected interfaces.

The **pcap** plugin supports the following options:

**interface** *interface*

> This option sets the interface that should be used.
> If this option is not set, or set to "any", the plugin will try
> to get packets from all interfaces.
> This may not work on certain platforms, such as Mac OS X.

**promiscuous** *true|false*

> Put the interface in promiscuous mode.

**ignore-source** *source*

> Ignore packets that originate from this address.

**ignore-destination** *destination*

> Ignore packets that has destination this address.

**filter** *expression*

> Process the packets on a network interface that match the boolean
> *expression* (see
> pcap-filter(7)
> for the expression syntax).

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple times in the **instance** block.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instance.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
