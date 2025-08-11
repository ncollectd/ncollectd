NCOLLECTD-ATS(5) - File Formats Manual

# NAME

**ncollectd-ats** - Documentation of ncollectd's ats plugin

# SYNOPSIS

	load-plugin ats
	plugin ats {
	    instance name {
	        url "http://host/path"
	        timeout seconds
	        label "key" "value"
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ats** plugin collect metris from Apache Traffic Server.

In the **plugin** block, there may be one or more **instance** blocks,
each defining a ATS instance.
The following options are valid within **instance** blocks:

**url** *http://host/path*

> URL of the ATS instance to retrieve metrics.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for HTTP requests to
> **url**, in seconds.
> By default, the configured **interval** is used to set the timeout.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
