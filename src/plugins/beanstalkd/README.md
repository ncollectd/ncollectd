NCOLLECTD-BEANSTALKD(5) - File Formats Manual

# NAME

**ncollectd-beanstalkd** - Documentation of ncollectd's beanstalkd plugin

# SYNOPSIS

	load-plugin beanstalkd
	plugin beanstalkd {
	    instance instance {
	        host hostname
	        port port
	        label "key" "value"
	        timeout seconds
	        interval seconds
	        filter {
	             ...
	        }
	    }
	}

# DESCRIPTION

The **beanstalkd** plugin connects to an instance of **beanstalkd** to
collect server statistics.

The configuration of the **beanstalkd** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
The following options are accepted within each **instance** block:

**host** *hostname*

> Hostname of the host running **beanstalkd** server.
> Defaults to *localhost*.

**port** *port*

> TCP port of server to connect to.
> Defaults to **11300**.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for the request to the
> beanstalkd server.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.
> By default the global **interval** setting will be used.

**filter** ...

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
