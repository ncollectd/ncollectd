NCOLLECTD-PODMAN(5) - File Formats Manual

# NAME

**ncollectd-podman** - Documentation of ncollectd's podman plugin

# SYNOPSIS

	load-plugin podman
	plugin podman {
	    instance instance {
	        url url
	        socket-path /path/to/unix/socket
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        verify-peer true|false
	        verify-host true|false
	        ca-cert file
	        client-cert /path/to/client-cert
	        client-key /path/to/client-key
	        client-key-pass key-pass
	        ssl-ciphers "list of ciphers"
	        labels key value
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **podman** plugin collect metrics from podman managed containers.

In the **plugin** block, there may be one or more **instance** blocks,
each defining a podman daemon.
The following options are valid within **instance** blocks:

**url** *url*

> Podman instance *url* to connect.
> When **socket-path** is set, the host part of the *url* is only used for the
> HTTP *Host* header and the *url* defaults to *http://localhost*.

**socket-path** */path/to/unix/socket*

> Will connect to the Unix domain socket instead of establishing a TCP connection
> to a host, e.g. */run/podman/podman.sock* or, for rootless Podman,
> *$XDG\_RUNTIME\_DIR/podman/podman.sock*.

**user** *username*

> Optional user name needed for authentication.

**user-env** *env-name*

> Get the user name needed for authentication from the environment variable
> *env-name*.

**password** *password*

> Optional password needed for authentication.

**password-env** *env-name*

> Get the password needed for authentication from the environment variable
> *env-name*.

**verify&#45;peer** **true|false**

> Enable or disable peer SSL certificate verification.
> See
> [http://curl.haxx.se/docs/sslcerts.html](http://curl.haxx.se/docs/sslcerts.html)
> for details.
> Enabled by default.

**verify-host** *true|false*

> Enable or disable peer host name verification.
> If enabled, the plugin checks if the *Common Name* or a
> *Subject Alternate Name* field of the SSL certificate matches the host
> name provided by the **url** option.
> If this identity check fails, the connection is aborted.
> Obviously, only works when connecting to a SSL enabled server.
> Enabled by default.

**ca-cert** *file*

> File that holds one or more SSL certificates.
> If you want to use HTTPS you will possibly need this option.
> What CA certificates come bundled with *libcurl* and are checked by
> default depends on the distribution you use.

**client-cert** */path/to/client-cert*

> File that holds the SSL certificate to be used for certificate-based
> authentication, as required by a Podman service started with
> **--tls-client-ca**.

**client-key** */path/to/client-key*

> File that holds the private key in PEM format to be used for certificate-based
> authentication.

**client-key-pass** *key-pass*

> Password required to load the private key in **client-key**.

**ssl-ciphers** *list of ciphers*

> Specifies which ciphers to use in the connection.
> The list of ciphers must specify valid ciphers.
> See
> [http://www.openssl.org/docs/apps/ciphers.html](http://www.openssl.org/docs/apps/ciphers.html)
> for details.

**labels** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for request to the podman
> instance, in seconds.
> By default, the configured **interval** is used to set the timeout.

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

