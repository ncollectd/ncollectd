NCOLLECTD-DOCKER(5) - File Formats Manual

# NAME

**ncollectd-docker** - Documentation of ncollectd's docker plugin

# SYNOPSIS

	load-plugin docker
	plugin docker {
	    interval interval {
	        url url
	        socket-path path
	        labels key value
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **docker** plugin collect statistics from docker running containers.

In the **plugin** block, there may be one or more **instance** blocks,
each defining a docker daemon.

The following options are valid within **instance** blocks:

**url** *url*

**socket-path** *path*

**labels** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for requests to the
> docker daemon in seconds.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

