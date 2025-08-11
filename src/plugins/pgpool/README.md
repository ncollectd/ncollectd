NCOLLECTD-PGPOOL(5) - File Formats Manual

# NAME

**ncollectd-pgpool** - Documentation of ncollectd's pgpool plugin

# SYNOPSIS

	load-plugin pgpool
	plugin pgpool {
	    instance instance {
	        host host
	        port port
	        database database
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        ssl-mode disable|allow|prefer|require
	        interval seconds
	        label key value
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **pgpool** plugin queries statistics from Pgpool-II server.
It keeps a persistent connection to all configured databases and tries to
reconnect if the connection has been interrupted.
A connection is configured by specifying a **instance** block as
described below.

**instance** *instance*

> The **instance** block defines one Pgpool-II server for which to collect
> statistics.

> **host** *host*

> > Specify the hostname or IP of the Pgpool-II server to connect to.
> > If the value begins with a slash, it is interpreted as the directory name
> > in which to look for the UNIX domain socket.

> **port** *port*

> > Specify the TCP port or the local UNIX domain socket file extension of the
> > server.

> **database** *database*

> > The database to connect.

> **user** *user*

> > Specify the username to be used when connecting to the server.

> **user-env** *env-name*

> > Get the username to be used when connecting to the server from the
> > environment variable *env-name*.

> **password** *password*

> > Specify the password to be used when connecting to the server.

> **password-env** *env-name*

> > Get the password to be used when connecting to the server from the
> > environment variable *env-name*.

> **ssl-mode** *disable|allow|prefer|require*

> > Specify whether to use an SSL connection when contacting the server.
> > The following modes are supported:

> > **disable**

> > > Do not use SSL at all.

> > **allow**

> > > First, try to connect without using SSL.
> > > If that fails, try using SSL.

> > **prefer** *(default)*

> > > First, try to connect using SSL.
> > > If that fails, try without using SSL.

> > **require**

> > > Use SSL only.

> **interval** *seconds*

> > Specify the interval with which the database should be queried.
> > The default is to use the global **interval** setting.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **database** block.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
