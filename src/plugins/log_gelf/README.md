NCOLLECTD-LOG\_GELF(5) - File Formats Manual

# NAME

**ncollectd-log\_gelf** - Documentation of ncollectd's log\_gelf plugin

# SYNOPSIS

	load-plugin log_gelf
	plugin log_gelf {
	    instance name {
	        log-level debug|info|notice|warning|err
	        host host
	        port port
	        packet-size size
	        ttl ttl
	        compress true|false
	    }
	}

# DESCRIPTION

The **log\_gelf** plugin receives log messages from the daemon and send
them in the Graylog Extended Log Format (GELF) via UDP.

The plugin has de following options:

**log-level** *debug|info|notice|warning|err*

> Sets the log-level.
> If, for example, set to **notice**, then all events with severity
> **notice**, **warning**, or **err** will be written
> to the logfile.

> Please note that **debug** is only available if ncollectd has been compiled
> with debugging support.

**host** *host*

> Set the destination address to send the metrics.
> Default to localhost.

**port** *port*

> Set the destionation port.
> The default port is 12201.

**packet-size** *size*

> Set maxinum packet size in bytes.
> Default to 1420 bytes.

**ttl** *ttl*

> Set the IP packet TTL.
> Default to 255.

**compress** *true|false*

> If the the playload is compressed.
> Default to true.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
