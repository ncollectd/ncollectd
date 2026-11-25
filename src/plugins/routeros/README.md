NCOLLECTD-ROUTEROS(5) - File Formats Manual

# NAME

**ncollectd-routeros** - Documentation of ncollectd's routeros plugin

# SYNOPSIS

	load-plugin routeros
	plugin routeros {
	    instance name {
	        host host
	        port port
	        ssl true|false
	        ca-cert /path/to/ca
	        verify-peer true|false
	        verify-host true|false
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **routeros** plugin connects to a device running *RouterOS*, the
Linux-based operating system for routers by *MikroTik*.
The configuration supports querying multiple routers:

The **routeros** plugin consists of one or more **instance** blocks.
Within each block, the following options are understood:

**host** *host*

> Hostname or IP-address of the router to connect to.

**port** *port*

> Port name or port number used when connecting.
> If left unspecified, "8728" is used, or "8729" (the *api-ssl* service)
> when **ssl** is enabled.

**ssl** *true|false*

> Connect to the *api-ssl* service of the router using TLS.
> Without TLS the user name, the password and all the data are sent in
> clear text over the network.
> The router must have a certificate assigned to the *api-ssl* service:
> without one RouterOS only offers anonymous ciphers, which are not accepted.
> Defaults to *false*.

**ca-cert** */path/to/ca*

> File with the PEM-encoded CA certificates used to verify the certificate
> of the router.
> If not set, the default CA certificates of the system are used.

**verify-peer** *true|false*

> Enable or disable the verification of the certificate of the router.
> Enabled by default.

**verify-host** *true|false*

> Enable or disable the check that the certificate of the router matches
> the host name or address given in **host**.
> Only used when **verify-peer** is enabled.
> Enabled by default.

**user** *user*

> Use the user name *user* to authenticate.
> Defaults to "admin".
> It is recommended to use a dedicated user with read-only rights
> (a group with the policies *read* and *api*) instead.

**user-env** *env-name*

> Get the user name *user* to authenticate from the environment
> variable *env-name*.

**password** *password*

> Set the password used to authenticate.

**password-env** *env-name*

> Get the password used to authenticate from the environment
> variable *env-name*.

**timeout** *seconds*

> Timeout for establishing the connection to the router and for every
> read or write on it, rounded to whole seconds (at least one second).
> If the router does not respond in time the connection is closed and
> reopened in the next interval.
> Defaults to half of the **interval**.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

