NCOLLECTD-NUT(5) - File Formats Manual

# NAME

**ncollectd-nut** - Documentation of ncollectd's nut plugin

# SYNOPSIS

	load-plugin nut
	plugin nut {
	    ca-path "/path/to/certs/folder"
	    instance name {
	        ups upsname@hostname[:port]
	        force-ssl true|false
	        verify-peer true|false
	        connect-timeout seconds
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **nut** plugin collects uninterruptible power supply (UPS) statistics
using the Network UPS Tools (NUT).
These statistics include basically everything NUT will give us, including
voltages, currents, power, frequencies, load, and temperatures.

**ca-path** */path/to/certs/folder*

> If **verify-peer** is set to *true*, this is required.
> Otherwise this is ignored.
> The folder pointed at must contain certificate(s) named according to their hash.
> Ex: XXXXXXXX.Y where X is the hash value of a cert and Y is 0.
> If name collisions occur because two different certs have the same hash value,
> it can be incremented in order to avoid conflict.
> To create a symbolic link to a certificate the following command can be used
> from within the directory where the cert resides:

> >   $ ln -s some.crt ./$(openssl x509 -hash -noout -in some.crt).0

> Alternatively, the package openssl-perl provides a command c\_rehash
> that will generate links like the one described above for ALL certs in a
> given folder.
> Example usage:

> >   $ c\_rehash /path/to/certs/folder

**instance** *name*

> **ups** *upsname@hostname*\[*:port*]

> > Add a UPS to ncollect data from.

> **force-ssl** *true|false*

> > Stops connections from falling back to unsecured if an SSL connection
> > cannot be established.
> > Defaults to *false*.

> **verify-peer** *true|false*

> > If set to *true*, requires a **ca-path** be provided.
> > Will use the **ca-path** to find certificates to use as Trusted Certificates
> > to validate a upsd server certificate.
> > If validation of the upsd server certificate fails, the connection will not be
> > established.
> > If **force-ssl** is undeclared or set to *false*, setting
> > **verify-peer** to true will override and set **force-ssl** to *true*.

> **connect-timeout** *seconds*

> > Sets the connect timeout.
> > By default, the configured **interval** is used to set the timeout.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **interval** *seconds*

> > Sets the interval (in seconds) in which the values will be collected
> > from this instance.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
