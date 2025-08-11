NCOLLECTD-SQUID(5) - File Formats Manual

# NAME

**ncollectd-squid** - Documentation of ncollectd's squid plugin

# SYNOPSIS

	load-plugin squid
	plugin squid {
	    instance name {
	        url url
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        digest true|false
	        verify-peer true|false
	        verify-host true|false
	        ca-cert /path/to/ca
	        header header
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **squid** plugin read Squid metrics over http orfrom an unix socket.

In the **plugin** block, there may be one or more **instance** blocks.
The following options are valid within **instance** blocks:

**url** *url*

> URL to access the Squid cachemgr.

**user** *user*

> Username to use if authorization is required to read the url.

**user-env** *env-name*

> Get the username to use if authorization is required to read the url from the
> environment variable *env-name*.

**password** *password*

> Password to use if authorization is required to read the url.

**password-env** *env-name*

> Get the password to use if authorization is required to read the url from the
> environment variable *env-name*.

**digest** *true|false*

> Enable HTTP digest authentication.

**verify-peer** *true|false*

> Enable or disable peer SSL certificate verification.
> See
> [http://curl.haxx.se/docs/sslcerts.html](http://curl.haxx.se/docs/sslcerts.html)
> for details.
> Enabled by default.

**verify-host** *true|false*

> Enable or disable peer host name verification.
> If enabled, the plugin checks if the Common Name or a
> Subject Alternate Name field of the SSL certificate matches the
> host name provided by the **url** option.
> If this identity check fails, the connection is aborted.
> Obviously, only works when connecting to a SSL enabled server.
> Enabled by default.

**ca-cert** */path/to/ca*

> File that holds one or more SSL certificates.
> If you want to use HTTPS you will possibly need this option.
> What CA certificates come bundled with libcurl and are checked by default
> depends on the distribution you use.

**header** *header*

> A HTTP header to add to the request.
> Multiple headers are added if this option is specified more than once.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> URL.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
