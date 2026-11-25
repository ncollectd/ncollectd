NCOLLECTD-NSD(5) - File Formats Manual

# NAME

**ncollectd-nsd** - Documentation of ncollectd's nsd plugin

# SYNOPSIS

	load-plugin nsd
	plugin nsd {
	    instance name {
	        host host
	        port port
	        socket-path path
	        server-cert file
	        control-key file
	        control-cert file
	        timeout seconds
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **nsd** plugin collect metrics from the NSD authoritative DNS Name Server.

The configuration consists of one or more **instance** blocks:

**host** *host*

> Hostname to connect.

**port** *port*

> Port to connect.

**socket-path** *path*

> Path of the NSD control unix socket. If set, **host** and **port** are ignored.

**server-cert** *file*

> Server certificate used to verify the server.
> Defaults to */etc/nsd/nsd\_server.pem*.

**control-key** *file*

> Private key of the client.
> Defaults to */etc/nsd/nsd\_control.key*.

**control-cert** *file*

> Certificate of the client.
> Defaults to */etc/nsd/nsd\_control.pem*.

> TLS is used when the **host** is not a local address or when any of
> **server-cert**, **control-key** or **control-cert** is set.

**timeout** *seconds*

> Timeout for the send and receive operations with the server.
> By default half of the **interval** is used.

**server-name** *servername*

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

