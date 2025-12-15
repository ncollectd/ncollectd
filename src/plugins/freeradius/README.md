NCOLLECTD-FREERADIUS(5) - File Formats Manual

# NAME

**ncollectd-freeradius** - Documentation of ncollectd's freeradius plugin

# SYNOPSIS

	load-plugin freeradius
	plugin freeradius {
	    instance instance {
	        host hostname
	        port port
	        secret secret
	        secret-env env-name
	        timeout seconds
	        label "key" "value"
	        interval seconds
	        filter {
	             ...
	        }
	    }
	}

# DESCRIPTION

The **freeradius** plugin connects to an instance of **freeradius**
to collect server statistics about certain operations it is doing, such as
the number of authentication and accounting requests, how many accepts and
failures, and server queue lengths.
Sending a specially-crafted RADIUS Status-Server packet to a "status"
virtual server.

The configuration of the **freeradius** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
The following options are accepted within each **instance** block:

**host** *hostname*

> Hostname of the host running **freeradius** "status" virtual server.
> Defaults to *localhost*.

**port** *port*

> TCP port of the "status" virtual server to connect to.
> Defaults to **18121**.

**secret** *secret*

> *Secret* used to calculate message-authenticator.

**secret-env** *env-name*

> Get the secret used to calculate message-authenticator from the environment
> variable *env-name*.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for the request to the
> freeradius server.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.
> By default the global **interval** setting will be used.

**filter** ...

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

