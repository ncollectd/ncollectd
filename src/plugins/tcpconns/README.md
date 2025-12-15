NCOLLECTD-TCPCONNS(5) - File Formats Manual

# NAME

**ncollectd-tcpconns** - Documentation of ncollectd's tcpconns plugin

# SYNOPSIS

	load-plugin tcpconns
	plugin tcpconns {
	    counter counter-name {
	        local-ip address|address/prefix
	        local-port port[,port][,start-end]...
	        remote-ip address|address/prefix
	        remote-port port[,port][,start-end]...
	    }
	}

# DESCRIPTION

The **tcpconns** plugin counts the number of currently established TCP
connections based on the local ip, local port, remote ip and/or remote port.

By default count all system connections grouped by tcp state.

You can define multiple *counters* blocks to fine-tune the connections
you are interested in:

**counter** *counter-name*

> **local-ip** *address*|*address*/*prefix*

> > Filter tcp connections for this local ip or network (with the CIDR notation).

> **local-port** *port*\[,*port*]\[,*start-end*]...

> > Filter tcp connections for this local port, a range of ports
> > (two ports separated by a dash) or list of ports (separated by a comma).

> **remote-ip** *address*|*address*/*prefix*

> > Filter tcp connections for this remote ip or network (with the CIDR notation).

> **remote-port** *port*\[,*port*]\[,*start-end*]...

> > Filter tcp connections for this repote port, a range of ports
> > (two ports separated by a dash) or list of ports (separated by a comma).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

