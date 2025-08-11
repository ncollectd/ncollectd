NCOLLECTD-REDIS(5) - File Formats Manual

# NAME

**ncollectd-redis** - Documentation of ncollectd's redis plugin

# SYNOPSIS

	load-plugin redis
	plugin redis {
	    instance name {
	        host hostname
	        port  port
	        socket path
	        timeout seconds
	        password password
	        password-env env-name
	        interval seconds
	        label key value
	        query query-string {
	            metric name
	            type gauge|counter|unknow
	            label key value
	            database index
	        }
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **redis** plugin connects to one or more Redis servers, gathers
information about each server's state and executes user-defined queries.
For each server there is a I&lt;Instance&gt; block which configures the connection
parameters and set of user-defined queries for this node.

The **instance** block identifies a new Redis node, that is a new Redis
instance running in an specified host and port.

**host** *hostname*

> The **host** option is the hostname or IP-address where the Redis instance
> is running on.

**port** *port*

> The **port** option is the TCP port on which the Redis instance accepts
> connections.
> Either a service name of a port number may be given.
> Please note that numerical port numbers must be given as a string, too.

**socket** *path*

> Connect to Redis using the UNIX domain socket at *path*.
> If this setting is given, the **host** and **port** settings are ignored.

**timeout** *seconds*

> The **timeout** option set the socket timeout for node response.
> Since the Redis read function is blocking, you should keep this value
> as low as possible.
> It is expected what **timeout** values should be lower than **interval**
> defined globally.
> Defaults to 2 seconds.

**password** *password*

> Use *password* to authenticate when connecting to *redis*.

**password-env** *env-name*

> Get the *password* to authenticate when connecting to *redis* from the
> environment variable *env-name*.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**query** *query-string*

> The **query** block identifies a query to execute against the redis server.
> There may be an arbitrary number of queries to execute.
> Each query should return single string or integer.

> **metric** *name*

> > Set the metric name.

> **type** *gauge|counter|unknow*

> > The **type** that's used for the metric.
> > Must be *gauge*, *counter*, or *unknow*.
> > If not set is unknow.
> > There must be exactly one **type** option inside each **Result** block.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **database** *index*

> > This index selects the Redis logical database to use for query.
> > Defaults to **0**.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
