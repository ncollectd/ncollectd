NCOLLECTD-WRITE\_REDIS(5) - File Formats Manual

# NAME

**ncollectd-write\_redis** - Documentation of ncollectd's write\_redis plugin

# SYNOPSIS

	load-plugin write_redis
	plugin write_redis {
	    instance name {
	        host hostname
	        port port
	        socket socket-path
	        password password
	        password-env env-name
	        timeout seconds
	        database database
	        retention seconds
	        store-rates true|false
	    }
	}

# DESCRIPTION

The **write\_redis** plugin writes metrics to a redis db with the
RedisTimeSeries module.

The plugin has the following options:

**host** *hostname*

> The **host** option is the hostname or IP-address where the Redis instance is
> running on.

**port** *port*

> The **port** option is the TCP port on which the Redis instance accepts
> connections.
> Either a service name of a port number may be given.

**socket** *socket-path*

> Connect to Redis using the UNIX domain socket at *path*.
> If this setting is given, the **host** and **port** settings are ignored.

**password** *password*

> Use *password* to authenticate when connecting to *redis*.

**password-env** *env-name*

> Get the password to authenticate from the environment variable *env-name*.

**timeout** *seconds*

**database** *database*

> This index selects the Redis logical database to use for query.
> Defaults to **0**.

**retention** *seconds*

> The maximum retention period for the metrics.

**store-rates** *true|false*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
