NCOLLECTD-POSTGRESQL(5) - File Formats Manual

# NAME

**ncollectd-postgresql** - Documentation of ncollectd's postgresql plugin

# SYNOPSIS

	load-plugin postgresql
	plugin postgresql {
	    query query-name {
	        statement sql
	        min-version version
	        max-version version
	        metric-prefix prefix
	        label key value
	        label-from key column-name
	        result {
	            type gauge|counter|info|unknown
	            type-from column-name
	            help help
	            help-from column-name
	            metric metric-name
	            metric-prefix  prefix
	            metric-from column-name
	            label key value
	            label-from key column-name
	            value-from column-name
	        }
	    }
	    instance instance {
	        database database
	        host host
	        port port
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        ssl-mode disable|allow|prefer|require
	        krb-srv-name name
	        service service
	        interval seconds
	        label key value
	        metric-prefix prefix
	        collect flags ...
	        query query-name
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **postgresql** plugin queries statistics from PostgreSQL databases.
It keeps a persistent connection to all configured databases and tries to
reconnect if the connection has been interrupted.
A database is configured by specifying a **instance** block as described
below.

By specifying custom database queries using a **query** block as described
below, you may collect any data that is available from some PostgreSQL
database.
This way, you are able to access statistics of external daemons
which are available in a PostgreSQL database or use future or special
statistics provided by PostgreSQL without the need to upgrade your ncollectd
installation.

**instance** *instance*

> The **instance** block defines one PostgreSQL database for which to collect
> statistics.
> It accepts a single mandatory argument which specifies the database name.
> None of the other options are required.
> PostgreSQL will use default values as documented in the section
> "CONNECTING TO A DATABASE" in the
> psql(1)
> manpage.
> However, be aware that those defaults may be influenced by
> the user collectd is run as and special environment variables.
> See the manpage for details.

> **host** *host*

> > Specify the hostname or IP of the PostgreSQL server to connect to.
> > If the value begins with a slash, it is interpreted as the directory name
> > in which to look for the UNIX domain socket.

> > This option is also used to determine the hostname that is associated with a
> > collected data set.
> > If it has been omitted or either begins with with a slash or equals
> > **localhost** it will be replaced with the global hostname definition
> > of ncollectd.
> > Any other value will be passed literally to collectd when dispatching values.
> > Also see the global **hostname** and **fqdn-lookup** options.

> **database** *database*

> **port** *port*

> > Specify the TCP port or the local UNIX domain socket file extension of the
> > server.

> **user** *user*

> > Specify the username to be used when connecting to the server.

> **user-env** *env-name*

> > Get the username to be used when connecting to the server from the environment
> > variable *env-name*.

> **password** *password*

> > Specify the password to be used when connecting to the server.

> **password-env** *env-name*

> > Get the password to be used when connecting to the server from the environment
> > variable *env-name*.

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

> **krb-srv-name** *name*

> > Specify the Kerberos service name to use when authenticating with Kerberos 5
> > or GSSAPI.
> > See the sections "Kerberos authentication" and "GSSAPI" of the
> > **PostgreSQL Documentation** for details.

> **service** *service*

> > Specify the PostgreSQL service name to use for additional parameters.
> > That service has to be defined in *pg\_service.conf* and holds additional
> > connection parameters.
> > See the section "The Connection Service File" in the
> > **PostgreSQL Documentation** for details.

> **interval** *seconds*

> > Specify the interval with which the database should be queried.
> > The default is to use the global **interval** setting.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **database** block.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics in the queries.

> **collect** *flags* ...

> > **settings**

> > > Read run-time parameters of the server from pg\_settings.

> > **database**

> > > Read database stats from pg\_stat\_database for all databases.

> > **database(database)**

> > > Read database stats from pg\_stat\_database for the specific database.

> > **database\_size**

> > > Read database size from pg\_database\_size for all databases.

> > **database\_size(database)**

> > > Read database size from pg\_database\_size for the specific database.

> > **database\_locks**

> > > Read database locks from pg\_locks for all databases.

> > **database\_locks(database)**

> > > Read database locks from pg\_locks for the specific database.

> > **database\_conflicts**

> > > Read database conflicts from pg\_stat\_database\_conflicts for
> > > all databases.

> > **database\_conflicts(database)**

> > > Read database conflicts from pg\_stat\_database\_conflicts for
> > > the specific database.

> > **database\_checkpointer**

> > > Read database checkpointer from pg\_stat\_checkpointer for
> > > all databases.

> > **database\_checkpointer(database)**

> > > Read database checkpointer from pg\_stat\_checkpointer for
> > > the specific database.

> > **activity**

> > **activity(database)**

> > **replication\_slots**

> > **replication\_slots(database)**

> > **replication**

> > **archiver**

> > **bgwriter**

> > > Read bgwriter stats from pg\_stat\_bgwriter.

> > **slru**

> > **io**

> > **table**

> > **table(schema)**

> > **table(schema,table)**

> > **table\_io**

> > **table\_io(schema)**

> > **table\_io(schema,table)**

> > **table\_size**

> > **table\_size(schema)**

> > **table\_size(schema,table)**

> > > Get table size using pg\_table\_size() and pg\_indexes\_size().
> > > An ACCESS EXCLUSIVE lock on the table will block ncollectd until lock
> > > is released.

> > **indexes**

> > **indexes(schema)**

> > **indexes(schema,table)**

> > **indexes(schema,table,index)**

> > **indexes\_io**

> > **indexes\_io(schema)**

> > **indexes\_io(schema,table)**

> > **indexes\_io(schema,table,index)**

> > **sequences\_io**

> > **sequences\_io(schema)**

> > **sequences\_io(schema,sequence)**

> > **functions**

> > **functions(schema)**

> > **functions(schema,function)**

> **query** *query-name*

> > Specifies a *query* which should be executed in the context of the database
> > connection.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5)

**query** *query-name*

> Query blocks define *SQL* statements and how the returned data should be
> interpreted.
> They are identified by the name that is given in the opening line of the block.
> Thus the name needs to be unique.
> Other than that, the name is not used in ncollectd.

> In each **query** block, there is one or more **result** blocks.
> **result** blocks define which column holds which value or instance
> information.
> You can use multiple **result** blocks to create multiple values from one
> returned row.
> This is especially useful, when queries take a long time and sending almost
> the same query again and again is not desirable.

> **statement** *sql*

> > Sets the statement that should be executed on the server.
> > This is **not** interpreted by ncollectd, but simply passed to the
> > database server.

> **min-version** *version*

> **max-version** *version*

> > Only use this query for the specified database version.
> > You can use these options to provide multiple queries with the same name
> > but with a slightly different syntax.
> > The plugin will use only those queries, where the specified minimum and
> > maximum versions fit the version of the database in use.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **query** block.

> **label-from** *key* *column-name*

> > Specifies the columns whose values will be used to create the labels.

> **result**

> > **type** *gauge|counter|info|unknown*

> > > The **type** that's used for each line returned.
> > > Must be *gauge*, *counter*, *info* or *unknown*.
> > > If not set is unknown.
> > > There must be exactly one **type** option inside each **Result** block.

> > **type-from** *column-name*

> > > Read the type from *column*.
> > > The column value must be *gauge*, *counter*,
> > > *info* or *unknown*.

> > **help** *help*

> > > Set the **help** text for the metric.

> > **help-from** *column-name*

> > > Read the **help** text for the the metric from the named column.

> > **metric** *metric-name*

> > > Set the metric name.

> > **metric-prefix** * prefix*

> > > Prepends *prefix* to the metric name in the **result**.

> > **metric-from** *column-name*

> > > Read the metric name from the named column.
> > > There must be at least one **metric** or **metric-from** option inside
> > > each **result** block.

> > **label** *key* *value*

> > > Append the label *key*=*value* to the submitting metrics.
> > > Can appear multiple times in the **result** block.

> > **label-from** *key* *column-name*

> > > Specifies the columns whose values will be used to create the labels.

> > **value-from** *column-name*

> > > Name of the column whose content is used as the actual data for the metric
> > > that are dispatched to the daemon.

> > > There must be only one **value-from** option inside each **result** block.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
