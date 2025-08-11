NCOLLECTD-OPENVPN(5) - File Formats Manual

# NAME

**ncollectd-openvpn** - Documentation of ncollectd's openvpn plugin

# SYNOPSIS

	load-plugin openvpn
	plugin openvpn {
	    instance name {
	        status-file /path/to/status
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **openvpn** plugin reads a status file maintained by OpenVPN and gathers
traffic statistics about connected clients.

To set up OpenVPN to write to the status file periodically, use the
--status option of OpenVPN.
So, in a nutshell you need:

	  $ openvpn $OTHER_OPTIONS --status "/var/run/openvpn-status" 10

The **instance** block has the following options:

**status-file** */path/to/status*

> Specifies the location of the status file.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
