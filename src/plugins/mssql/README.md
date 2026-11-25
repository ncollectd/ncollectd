NCOLLECTD-MSSQL(5) - File Formats Manual

# NAME

**ncollectd-mssql** - Documentation of ncollectd's mssql plugin

# SYNOPSIS

	load-plugin mssql
	plugin mssql {
	    query query-name {
	        statement sql
	        min-version version
	        max-version version
	        metric-prefix prefix
	        label key value
	        label-from key column-name
	        result {
	            type gauge|counter
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
	    instance name {
	        server server
	        database database
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        label key value
	        metric-prefix prefix
	        interval seconds
	        timeout seconds
	        ping-query query
	        query query-name
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **mssql** plugin connect to a MS SQL database, execute *SQL*
statements and read back the results.
You can configure how each column is to be interpreted and the
plugin will generate one or more data sets from each row returned according
to these rules.

**instance** *name*

> Instance blocks define a connection to a database and which queries should be
> sent to that database.
> Each instance block needs a "name" as string argument in the starting tag of
> the block.

> **server** *server*

> > The server name of the database to connect to.
> > If the name is found in freetds.conf(5), the settings of that entry
> > (host, port, tds version, ...) are used.
> > The plugin has no TLS options: set **encryption** to *require* (and
> > **ca file** to verify the server certificate) in that entry to enforce
> > encrypted connections.

> **database** *database*

> > Select the database.
> > Defaults to *no database*.

> **user** *user*

> > User to connect to database.
> > If no user is set, an empty user name is sent, which FreeTDS uses to
> > request Kerberos authentication.

> **user-env** *env-name*

> > Get the user to connect to database from the environment variable
> > *env-name*.

> **password** *password*

> > User password to connect to database

> **password-env** *env-name*

> > Get the user password to connect to database from the environment variable
> > *env-name*.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **database** block.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics name.

> **interval** *seconds*

> > Sets the interval (in seconds) in which the values will be collected from this
> > database.
> > By default the global **interval** setting will be used.

> **timeout** *seconds*

> > Sets the timeout, in seconds, to log in to the server and to wait for the
> > response of a query.
> > When it expires the connection is closed and opened again in the next read.
> > By default, the configured **interval** is used to set the timeout.

> **ping-query** *query*

> > Specifies a query to test the connection to the database.

> **query** *query-name*

> > Associates the query named *query-name* with this database connection.
> > The query needs to be defined *before* this statement, i. e. all query
> > blocks you want to refer to must be placed above the database block you want to
> > refer to them from.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5).

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
> > This is **not** interpreted by ncollectd, but simply passed to the database
> > server.
> > Only the first result set returned by the statement is used, the results
> > of statements without columns (e.g. **SET NOCOUNT ON**) are skipped.

> **min-version** *version*

> **max-version** *version*

> > Only use this query for the specified database version.
> > You can use these options to provide multiple queries with the same name but
> > with a slightly different syntax.
> > The plugin will use only those queries, where the specified minimum
> > and maximum versions fit the version of the database in use.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **query** block.

> **label-from** *key* *column-name*

> > Specifies the columns whose values will be used to create the labels.

> **result**

> > **type** *gauge*|*counter*

> > > The **type** that's used for each metric returned.
> > > Must be *gauge* or *counter*.
> > > If not set is unknown.
> > > There must be exactly one **type** option inside each **Result** block.

> > **type-from** *column-name*

> > > Read the type from *column*.
> > > The column value must be *gauge* or *counter*.

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
ncollectd.conf(5),
freetds.conf(5)

