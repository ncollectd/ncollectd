NCOLLECTD-CUPS(5) - File Formats Manual

# NAME

**ncollectd-cups** - Documentation of ncollectd's cups plugin

# SYNOPSIS

	load-plugin cups
	plugin cups {
	    instance instance {
	        host host
	        port port
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **cups** plugin collect printer and jobs metrics from a cups server.

The plugin configuration consists of one or more **instance** blocks which
specify one *cups* connection each.
Within the **instance** blocks, the following options are allowed:

**host** *host*

> Hostname or address to connect to.
> The default server comes from the CUPS\_SERVER environment variable,
> then the *~/.cups/client.conf* file, and finally the
> */etc/cups/client.conf* file.
> If not set, the default is the local system - either localhost or
> a domain socket path.

**port** *port*

> Service name or port number to connect to.
> Defaults to 631.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
