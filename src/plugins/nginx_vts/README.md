NCOLLECTD-NGINX\_VTS(5) - File Formats Manual

# NAME

**ncollectd-nginx\_vts** - Documentation of ncollectd's nginx\_vts plugin

# SYNOPSIS

	load-plugin nginx_vts
	plugin nginx_vts
	    instance instance {
	        url  url
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        verify-peer true|false
	        verify-host true|false
	        ca-cert file
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
> Redirects are only followed to *http* and *https* URLs.

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
> name provided by the **URL** option.
> If this identity check fails, the connection is aborted.
> Obviously, only works when connecting to a SSL enabled server.
> Enabled by default.

**ca-cert** *file*

> File that holds one or more SSL certificates.
> If you want to use HTTPS you will possibly need this option.
> What CA certificates come bundled with *libcurl* and are checked by
> default depends on the distribution you use.

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

