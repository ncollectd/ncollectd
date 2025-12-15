NCOLLECTD-NGINX(5) - File Formats Manual

# NAME

**ncollectd-nginx** - Documentation of ncollectd's nginx plugin

# SYNOPSIS

	load-plugin nginx
	plugin nginx {
	    instance "host" {
	        url "http://host/nginx_status"
	        socket-path "/path/to/unix/socket"
	        user "username"
	        user-env "env-name"
	        password "password"
	        password-env "env-name"
	        verify-peer true|false
	        verify-host true|false
	        ca-cert "file"
	        ssl-ciphers "list of ciphers"
	        timeout milliseconds
	        label "key" "value"
	        interval interval
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **nginx** plugin collects the number of connections and requests handled
by the *nginx daemon*, a HTTP and mail server/proxy.
It queries the page provided by the **ngx\_http\_stub\_status\_module** module,
which isn't compiled by default.

You can use the following snipped to base your Ningx config upon:

	location = /nginx_status {
	    stub_status;
	}

Please refer to
[http://nginx.org/en/docs/http/ngx\_http\_stub\_status\_module.html](http://nginx.org/en/docs/http/ngx_http_stub_status_module.html)
for more information on how to compile and configure nginx and this module.

The configuration of the **nginx** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
For example:

	plugin nginx {
	    instance "www1" {
	        url "http://www1.example.com/nginx_status"
	    }
	    instance "www2" {
	        url "http://www2.example.com/nginx_status"
	    }
	}

The instance name will be used as *instance* label in the metrics.
In order for the plugin to work correctly, each instance name must be unique.
This is not enforced by the plugin and it is your responsibility to ensure it.

The following options are accepted within each **instance** block:

**url** *http://host/nginx\_status*

> Sets the URL of the **ngx\_http\_stub\_status\_module** output.

**socket-path** */path/to/unix/socket*

> Will connect to the Unix domain socket instead of establishing a TCP connection
> to a host.

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

**ssl-ciphers** *list of ciphers*

> Specifies which ciphers to use in the connection.
> The list of ciphers must specify valid ciphers.
> See
> [http://www.openssl.org/docs/apps/ciphers.html](http://www.openssl.org/docs/apps/ciphers.html)
> for details.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for HTTP requests to
> **URL**, in seconds.
> By default, the configured **interval** is used to set the timeout.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> URL.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# DEPENDENCIES

libcurl(3)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

