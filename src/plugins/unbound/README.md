NCOLLECTD-UNBOUND(5) - File Formats Manual

# NAME

**ncollectd-unbound** - Documentation of ncollectd's unbound plugin

# SYNOPSIS

	load-plugin unbound
	plugin unbound {
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

The **unbound** plugin collect metrics from the Unbound resolver.

The configuration consists of one or more **instance** blocks:

**host** *host*

> Hostname of the unbound remote control interface to connect.
> If the host is not `localhost`, `127.0.0.1` or `::1` the
> connection uses TLS.

**port** *port*

> Port to connect, the default is `8953`.

**socket-path** *path*

> Connect to the unbound remote control unix socket in *path* instead of
> **host** and **port**.

**server-cert** *file*

> File with the server certificate used to verify the server in TLS connections.
> For remote hosts the default is `/etc/unbound/unbound_server.pem`.

**control-key** *file*

> File with the client private key for TLS connections.
> If given for a local host the connection uses TLS.
> For remote hosts the default is `/etc/unbound/unbound_control.key`.

**control-cert** *file*

> File with the client certificate for TLS connections.
> For remote hosts the default is `/etc/unbound/unbound_control.pem`.

**timeout** *seconds*

> Timeout for the connection to unbound.

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

