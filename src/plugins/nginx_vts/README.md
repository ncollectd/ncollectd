NCOLLECTD-NGINX\_VTS(5) - File Formats Manual

# NAME

**ncollectd-nginx\_vts** - Documentation of ncollectd's nginx\_vts plugin

# SYNOPSIS

	load-plugin nginx_vts
	plugin nginx_vts
	    instance instance {
	        url  url
	        labels key value
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **nginx\_vts** collect the metrics exposed from the Nginx virtual
host traffic status module.

**url** *url*

> Sets the URL of the Nginx virtual host traffic status module.

**labels** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for HTTP requests
> to **URL**, in seconds.
> By default, the configured **interval** is used to set the timeout.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this URL.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

