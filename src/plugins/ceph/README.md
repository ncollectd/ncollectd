NCOLLECTD-CEPH(5) - File Formats Manual

# NAME

**ncollectd-ceph** - Documentation of ncollectd's ceph plugin

# SYNOPSIS

	load-plugin ceph
	plugin ceph {
	    daemon deamon-name {
	        socket-path admin-socket
	        label key value
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ceph** plugin collects values retrieved from ceph daemon admin sockets.

A separate **daemon** block must be configured for each ceph daemon to be
monitored.
The following example will read daemon statistics from four separate ceph
daemons running on the same device (two OSDs, one MON, one MDS):

	plugin ceph {
	    daemon "osd.0" {
	        socket-path "/var/run/ceph/ceph-osd.0.asok"
	    }
	    daemon "osd.1" {
	        socket-path "/var/run/ceph/ceph-osd.1.asok"
	    }
	    daemon "mon.a" {
	        socket-path "/var/run/ceph/ceph-mon.ceph1.asok"
	    }
	    daemon "mds.a" {
	        socket-path "/var/run/ceph/ceph-mds.ceph1.asok"
	    }
	}

Each **daemon** block must have a string argument.
A **socket-path** is also required for each **daemon** block:

**socket-path** *admin-socket*

> Specifies the path to the UNIX admin socket of the ceph daemon.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**timeout** *seconds*

> Set the timeout to connect to the admin socket.
> Default to **1**.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this daemon.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

