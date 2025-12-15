NCOLLECTD-CERT(5) - File Formats Manual

# NAME

**ncollectd-cert** - Documentation of ncollectd's cert plugin

# SYNOPSIS

	load-plugin cert
	plugin cert {
	    instance name {
	        host host
	        port port
	        server-name servername
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **cert** plugin obtain the date of certification expiration.

The configuration to obtain the certificate expiration data is put
in a **instance** block:

**host** *host*

> Hostname to connect.

**port** *port*

> Port to connect.

**server-name** *servername*

> Set the server name at the start of the handshaking process in the
> TLS connection.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

