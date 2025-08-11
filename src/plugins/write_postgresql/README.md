NCOLLECTD-WRITE\_POSTGRESQL(5) - File Formats Manual

# NAME

**ncollectd-write\_postgresql** - Documentation of ncollectd's write\_postgresql plugin

# SYNOPSIS

	load-plugin write postgresql
	plugin write_postgresql {
	    instance instance {
	        host host
	        port port
	        database database
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        ssl-mode disable|allow|prefer|require
	        krb-srv-name name
	        service service
	        interval seconds
	        commit-interval seconds
	        write metrics|notifications
	        statement sql-statement
	    }
	}

# DESCRIPTION

The **write\_postgresql** plugin writes metrics in a PostgreSQL databases.
It keeps a persistent connection to all configured databases and tries to
reconnect if the connection has been interrupted.
A database is configured by specifying a **instance** block as described
below.

The following options are accepted within each **instance** block:

**host** *host*

> Specify the hostname or IP of the PostgreSQL server to connect to.
> If the value begins with a slash, it is interpreted as the directory name in
> which to look for the UNIX domain socket.

**port** *port*

> Specify the TCP port or the local UNIX domain socket file extension of the
> server.

**database** *database*

> Database name to connect.

**user** *user*

> Specify the username to be used when connecting to the server.

**user-env** *env-name*

> Get the username to be used when connecting to the server from the environment
> variable
> *env-name*.

**password** *password*

> Specify the password to be used when connecting to the server.

**password-env** *env-name*

> Get the the password to be used when connecting to the server from the
> environment variable
> *env-name*.

**ssl-mode** *disable|allow|prefer|require*

> Specify whether to use an SSL connection when contacting the server.
> The following modes are supported:

> **disable**

> > Do not use SSL at all.

> **allow**

> > First, try to connect without using SSL.
> > If that fails, try using SSL.

> **prefer** *(default)*

> > First, try to connect using SSL.
> > If that fails, try without using SSL.

> **require**

> > Use SSL only.

**krb-srv-name** *name*

> Specify the Kerberos service name to use when authenticating with Kerberos 5
> or GSSAPI.
> See the sections "Kerberos authentication" and "GSSAPI" of the
> **PostgreSQL Documentation** for details.

**service** *service*

> Specify the PostgreSQL service name to use for additional parameters.
> That service has to be defined in *pg\_service.conf* and holds additional
> connection parameters.
> See the section "The Connection Service File" in the
> **PostgreSQL Documentation** for details.

**interval** *seconds*

> Specify the interval with which the full callback in called to commit the data.
> The default is to use the global **interval** setting.

**commit-interval** *seconds*

> This option causes a writer to put several updates into a single transaction.
> This transaction will last for the specified amount of time.
> By default, each SQL statement will be executed in a separate transaction.
> Each transaction generates a fair amount of overhead which can, thus,
> be reduced by activating this option.

**write** *metrics|notifications*

> If set to *metrics* (the default) the plugin will handle metrics.
> If set to *notifications* the plugin will handle notifications.

**statement** *sql-statement*

> This mandatory option specifies the SQL statement that will be executed
> for each submitted value.
> A single SQL statement is allowed only.
> Anything after the first semicolon will be ignored.

> **Metrics**

> > Four parameters will be passed to the statement and should be specified
> > as tokens $1, $2, through $4 in the statement string.
> > In general, it is advisable to create and call a custom function in the
> > PostgreSQL database for this purpose.

> > The following values are made available through those parameters:

> > **$1**

> > > The metric name.

> > **$2**

> > > The metric labels as an array of pairs.

> > **$3**

> > > The value of the metric.

> > **$4**

> > > The timestamp of the queried value as an RFC 3339-formatted local time.

> **Notifications**

> > Five parameters will be passed to the statement and should be specified as
> > tokens $1, $2, through $5 in the statement string.
> > In general, it is advisable to create and call a custom function in the
> > PostgreSQL database for this purpose.

> > The following values are made available through those parameters:

> > **$1**

> > > The notification name.

> > **$2**

> > > The notification labels as an array of pairs.

> > **$3**

> > > The notification annotations as an array of pairs.

> > **$4**

> > > The severity of the notification as string:
> > > *FAILURE*, *WARNING* or *OKAY*

> > **$5**

> > > The timestamp of the notification as an RFC 3339-formatted local time.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
