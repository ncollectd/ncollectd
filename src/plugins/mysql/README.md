NCOLLECTD-MYSQL(5) - File Formats Manual

# NAME

**ncollectd-mysql** - Documentation of ncollectd's mysql plugin

# SYNOPSIS

	load-plugin mysql
	plugin mysql {
	    query query-name {
	        statement sql
	        min-version version
	        max-version version
	        metric-prefix prefix
	        label key value
	        label-from key column-name
	        result {
	            type gauge|counter|info|unknow
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
	        host hostname
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        port port
	        socket socket-path
	        database database
	        ssl-key "/path/to/key.pem"
	        ssl-cert "/path/to/cert.pem"
	        ssl-ca "/path/to/ca.pem"
	        ssl-ca-path "/path/to/cas/"
	        ssl-cipher cyphers
	        connect-timeout seconds
	        heartbeat-utc  true|false
	        heartbeat-schema schema
	        heartbeat-table table
	        interval seconds
	        collect flags ...
	        metric-prefix prefix
	        label key value
	        query query-name
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **mysql** plugin requires **mysqlclient** to be installed.
It connects to one or more databases when started and keeps the connection
up as long as possible.
When the connection is interrupted for whatever reason it will try
to re-connect.
The plugin will complain loudly in case anything goes wrong.

**instance** *name*

> A **instance** block defines one connection to a MySQL database.
> It accepts a single argument which specifies the name of the database.
> None of the other options are required.
> MySQL will use default values as documented in the
> mysql\_real\_connect() and mysql\_ssl\_set() sections in the
> **MySQL reference manual**.
> Each database needs a *instance-name* as string argument in the starting
> tag of the block.

> **host** *hostname*

> > Hostname of the database server.
> > Defaults to **localhost**.

> **user** *username*

> > Username to use when connecting to the database.
> > The user does not have to be granted any privileges
> > (which is synonym to granting the USAGE privilege),
> > unless you want to collect replication statistics.

> **user-env** *env-name*

> > Get the username to use when connecting to the database from the environment
> > variable *env-name*.

> **password** *password*

> > Password needed to log into the database.

> **password-env** *env-name*

> > Get the password needed to log into the database from the environment
> > variable *env-name*.

> **port** *port*

> > TCP-port to connect to.
> > If **host** is set to **localhost** (the default), this setting
> > has no effect.
> > See the documentation for the mysql\_real\_connect function for details.

> **socket** *socket-path*

> > Specifies the path to the UNIX domain socket of the MySQL server.
> > This option only has any effect, if **host** is set to
> > **localhost** (the default).
> > Otherwise, use the **port** option above.
> > See the documentation for the
> > mysql\_real\_connect function for details.

> **database** *database*

> > Select this database.
> > Defaults to *no database* which is a perfectly reasonable
> > option for what this plugin does.

> **ssl-key** */path/to/key.pem*

> > If provided, the X509 key in PEM format.

> **ssl-cert** */path/to/cert.pem*

> > If provided, the X509 cert in PEM format.

> **ssl-ca** */path/to/ca.pem*

> > If provided, the CA file in PEM format (check OpenSSL docs).

> **ssl-ca-path** */path/to/cas/*

> > If provided, the CA directory (check OpenSSL docs).

> **ssl-cipher** *cyphers*

> > If provided, the SSL cipher to use.

> **connect-timeout** *seconds*

> > Sets the connect timeout for the MySQL client.

> **heartbeat-utc** *true|false*

> > Use UTC for timestamps of the current server.
> > Defauilt if *false*.

> **heartbeat-schema** *schema*

> > Schema from where to collect heartbeat data.
> > Default is *heartbeat*.

> **heartbeat-table** *table*

> > Table from where to collect heartbeat data.
> > Default is *heartbeat*.

> **interval** *seconds*

> > Sets the interval (in seconds) in which the values will be
> > collected from this database.

> **collect** *flags* ...

> > **globals**

> > > Collect global values from SHOW GLOBAL STATUS command.

> > **acl**

> > > Collect Acl\_\* values from SHOW GLOBAL STATUS command.

> > **aria**

> > > Collect Aria\_\* values from SHOW GLOBAL STATUS command.

> > **binlog**

> > > Collect Binlog\_\* values from SHOW GLOBAL STATUS command.

> > **commands**

> > > Collect Com\_\* values from SHOW GLOBAL STATUS command.

> > **features**

> > > Collect Feature\_\* values from SHOW GLOBAL STATUS command.

> > **handlers**

> > > Collect Handler\_\* values from SHOW GLOBAL STATUS command.

> > **innodb**

> > > Collect values from INFORMATION\_SCHEMA.INNODB\_METRICS table.

> > **innodb\_cmp**

> > > Collect values from INFORMATION\_SCHEMA.INNODB\_CMPMEM table.

> > **innodb\_cmpmem**

> > > Collect values from INFORMATION\_SCHEMA.INNODB\_CMP table.

> > **innodb\_tablespace**

> > > Collect values from INFORMATION\_SCHEMA.INNODB\_SYS\_TABLESPACES table.

> > **myisam**

> > > Collect Key\_\* values from SHOW GLOBAL STATUS command.

> > **perfomance\_lost**

> > > Collect Performance\_schema\_\* values
> > > from SHOW GLOBAL STATUS command.

> > **qcache**

> > > Collect Qcache\_\* values from SHOW GLOBAL STATUS command.

> > **slave**

> > > Collect Slave\* values from SHOW GLOBAL STATUS command.

> > **ssl**

> > > Collect Ssl\_\* values from SHOW GLOBAL STATUS command.

> > **wsrep**

> > > Enable the collection of wsrep plugin statistics, used in Master-Master
> > > replication setups like in MySQL Galera/Percona XtraDB Cluster.
> > > User needs only privileges to execute 'SHOW GLOBAL STATUS'.
> > > Defaults to **false**.

> > **client**

> > > Collect values from INFORMATION\_SCHEMA.CLIENT\_STATISTICS table.

> > **user**

> > > Collect values from INFORMATION\_SCHEMA.USER\_STATISTICS table.

> > **index**

> > > Collect values from INFORMATION\_SCHEMA.INDEX\_STATISTICS table.

> > **table**

> > > Collect values from INFORMATION\_SCHEMA.TABLE\_STATISTICS table.

> > **table**

> > > Collect values from INFORMATION\_SCHEMA.TABLES table.

> > **response\_time**

> > > Collect values from INFORMATION\_SCHEMA.QUERY\_RESPONSE\_TIME table.
> > > In Percona server collect values fron
> > > INFORMATION\_SCHEMA.QUERY\_RESPONSE\_TIME\_READ
> > > and INFORMATION\_SCHEMA.QUERY\_RESPONSE\_TIME\_WRITE and table.

> > **master**

> > **slave**

> > > Enable the collection of primary / replica statistics in a replication setup.
> > > In order to be able to get access to these statistics, the user needs special
> > > privileges.

> > **heartbeat**

> > > Collect replication delay measured by a heartbeat mechanism.
> > > The reference implementation supported is **pt-heartbeat**.
> > > You can control the table name with **heartbeat-schema**
> > > and **heartbeat-table** options.
> > > The heartbeat table must have at least this two columns:

> > > >   CREATE TABLE heartbeat (
> > > >       ts        varchar(26)  NOT NULL,
> > > >       server\_id int unsigned NOT NULL PRIMARY KEY,
> > > >   );

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **database** block.

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
> > This is **not** interpreted by ncollectd, but simply passed to the
> > database server.
> > Therefore, the SQL dialect that's used depends on the server collectd
> > is connected to.

> **min-version** *version*

> **max-version** *version*

> > Only use this query for the specified database version.
> > You can use these options to provide multiple queries with the same name
> > but with a slightly different syntax.
> > The plugin will use only those queries, where the specified
> > minimum and maximum versions fit the version of the database in use.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **query** block.

> **label-from** *key* *column-name*

> > Specifies the columns whose values will be used to create the labels.

> **result**

> > **type** *gauge|counter|info|unknow*

> > > The **type** that's used for each line returned.
> > > Must be *gauge*, *counter*, *info* or *unknow*.
> > > If not set is unknow.
> > > There must be exactly one **type** option inside each **result** block.

> > **type-from** *column-name*

> > > Read the type from *column*.
> > > The column value must be *gauge*, *counter*, *info* or *unknow*.

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
