NCOLLECTD-ZOOKEEPER(5) - File Formats Manual

# NAME

**ncollectd-zookeeper** - Documentation of ncollectd's zookeeper plugin

# SYNOPSIS

	load-plugin zookeeper
	plugin zookeeper {
	    instance name {
	        host host
	        port service
	        label key value
	        interval interval
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **zookeeper** plugin will collect statistics from a *zookeeper*
server using the *mntr* command.
It requires Zookeeper 3.4.0+ and access to the client port.

The plugin configuration consists of one or more **instance** blocks which
specify one *zookeper* connection each.
Within the **instance** blocks, the following options are allowed:

**host** *host*

> Hostname or address to connect to.
> Defaults to localhost.

**port** *service*

> Service name or port number to connect to.
> Defaults to 2181.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *interval*

> Sets the interval (in seconds) in which the values will be collected from this
> instance.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

