NCOLLECTD-ROUTEROS(5) - File Formats Manual

# NAME

**ncollectd-routeros** - Documentation of ncollectd's routeros plugin

# SYNOPSIS

	load-plugin routeros
	plugin routeros {
	    instance name {
	        host host
	        port port
	        user user
	        user-env env-name
	        password password
	        password-env env-name
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
> If left unspecified, the default will be chosen by *librouteros*,
> currently "8728".
> This option expects a string argument, even when a numeric port number is given.

**user** *user*

> Use the user name *user* to authenticate.
> Defaults to "admin".

**user-env** *env-name*

> Get the user name *user* to authenticate from the environment
> variable *env-name*.

**password** *password*

> Set the password used to authenticate.

**password-env** *env-name*

> Get the password used to authenticate from the environment
> variable *env-name*.

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

