NCOLLECTD-DOCKER(5) - File Formats Manual

# NAME

**ncollectd-docker** - Documentation of ncollectd's docker plugin

# SYNOPSIS

	load-plugin docker
	plugin docker {
	    instance name {
	        url url
	        socket-path path
	        ca-cert /path/to/ca-cert
	        client-key /path/to/client-key
	        client-cert /path/to/client-cert
	        label key value
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **docker** plugin collects statistics from docker running containers.
It uses version 1.40 of the Docker Engine API (Docker 19.03 or later).

In the **plugin** block, there may be one or more **instance** blocks,
each defining a docker daemon.

The following options are valid within **instance** blocks:

**url** *url*

> Base URL of the docker daemon API, for example *http://127.0.0.1:2375*
> or *https://127.0.0.1:2376*; the API path is appended to it.
> When **socket-path** is set the connection is made through the socket.

**socket-path** *path*

> Path of the UNIX socket of the docker daemon, usually
> */var/run/docker.sock*.
> At least one of **url** or **socket-path** must be set.

**ca-cert** */path/to/ca-cert*

> File that holds one or more SSL certificates used to verify the docker
> daemon when **url** uses HTTPS.

**client-key** */path/to/client-key*

> File that holds the private key in PEM format to be used for certificate-based
> authentication, as required by a docker daemon started with **--tlsverify**.

**client-cert** */path/to/client-cert*

> File that holds the SSL certificate to be used for certificate-based
> authentication.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for the requests that list
> the containers and the connection timeout of the statistics streams, in
> seconds.
> By default the collection interval is used.

**interval** *seconds*

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

