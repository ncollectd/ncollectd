NCOLLECTD-BIND(5) - File Formats Manual

# NAME

**ncollectd-bind** - Documentation of ncollectd's bind plugin

# SYNOPSIS

	load-plugin bind
	plugin bind {
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

Starting with BIND 9.5.0, it provides extensive statistics about queries,
responses and lots of other information.
The bind plugin retrieves this information that's encoded in XML or
JSON and provided via HTTP and submits the values to ncollectd.
To use this plugin, you first need to tell BIND to make this information
available.
This is done with the statistics-channels configuration option:

	statistics-channels {
	    inet localhost port 8053;
	};

In the **plugin** block, there may be one or more **instance** blocks,
each defining a BIND instance.
The following options are valid within **instance** blocks:

**url** *http://host/path*

> URL of the BIND instance to retrieve metrics.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for HTTP requests
> to **url**, in seconds.
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
