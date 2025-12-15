NCOLLECTD-FCGI(5) - File Formats Manual

# NAME

**ncollectd-fcgi** - Documentation of ncollectd's fcgi plugin

# SYNOPSIS

	load-plugin fcgi
	plugin fcgi {
	    instance instance {
	        host hostname
	        port port-number
	        socket-path path-to-socket
	        metric-prefix metric
	        label key value
	        measure-response-time true|false
	        measure-response-code true|false
	        param key value
	        data data
	        interval seconds
	        timeout seconds
	        match type {
	            ...
	        }
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **fcgi** plugin can connect to a fcgi application to use the match
infrastructure to obtain metrics from data.
In the **plugin** block, there may be one or more **instance** blocks,
each defining a fcgi application and one or more "matches" to be performed
on the returned data.
The string argument to the **instance** block is used as instance label.

The following options are valid within **instance** blocks:

**host** *hostname*

**port** *port-number*

**socket-path** *path-to-socket*

**metric-prefix** *metric*

> Prepends *prefix* to the metrics name in the **match** block.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**measure-response-time** *true|false*

> Measure response time for the request.
> If this setting is enabled, **match** blocks (see below) are optional.
> Disabled by default.

**measure-response-code** *true|false*

> Measure response code for the request.
> If this setting is enabled, **match** blocks (see below) are optional.
> Disabled by default.

**param** *key* *value*

> Sets a parameter that will be passed to the FastCGI server.

**data** *data*

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for FCGI requests,
> in seconds.
> By default, the configured **interval** is used to set the timeout.

**match** *type*

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

