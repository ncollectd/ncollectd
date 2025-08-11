NCOLLECTD-SCRAPER(5) - File Formats Manual

# NAME

**ncollectd-scraper** - Documentation of ncollectd's scraper plugin

# SYNOPSIS

	load-plugin scraper
	plugin scraper {
	    instance name {
	        url url
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        digest true|false
	        verify-peer true|false
	        verify-host true|false
	        ca-cert /path/to/ca-cert
	        header header
	        post data
	        interval seconds
	        timeout seconds
	        label key value
	        metric-prefix prefix
	        collect flags
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The scraper plugin collect metrics from a http endpoint.

**url** *url*

> URL of the web site to retrieve the metrics.

**user** *username*

> Username to use if authorization is required to read the page.

**user-env** *env-name*

> Get the username to use if authorization is required to read the page from the
> environment variable *env-name*.

**password** *password*

> Password to use if authorization is required to read the page.

**password-env** *env-name*

> Get the password to use if authorization is required to read the page from the
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
> Subject Alternate Name field of the SSL certificate matches the host
> name provided by the **url** option.
> If this identity check fails, the connection is aborted.
> Obviously, only works when connecting to a SSL enabled server.
> Enabled by default.

**ca-cert** */path/to/ca-cert*

> File that holds one or more SSL certificates.
> If you want to use HTTPS you will possibly need this option.
> What CA certificates come bundled with **libcurl** and are checked by default
> depends on the distribution you use.

**header** *header*

> A HTTP header to add to the request.
> Multiple headers are added if this option is specified more than once.

**post** *data*

> Specifies that the HTTP operation should be a POST instead of a GET.
> The complete data to be posted is given as the argument.
> This option will usually need to be accompanied by a **header**
> option to set an appropriate Content-Type for the post
> body (e.g. to application/x-www-form-urlencoded).

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for HTTP requests
> to **url**, in seconds.
> By default, the configured **interval** is used to set the timeout.
> If **timeout** is bigger than the **interval**, keep in mind that each slow
> network connection will stall one read thread.
> Adjust the **read-threads** global setting accordingly to prevent this from
> blocking other plugins.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**metric-prefix** *prefix*

> Prepends *prefix* to the metric name in the **match**.

**collect** *flags*

> **total\_time**

> > Total time of the transfer, including name resolving, TCP connect, etc.

> **namelookup\_time**

> > Time it took from the start until name resolving was completed.

> **connect\_time**

> > Time it took from the start until the connect to the remote host (or proxy)
> > was completed.

> **pretransfer\_time**

> > Time it took from the start until just before the transfer begins.

> **size\_upload**

> > The total amount of bytes that were uploaded.

> **size\_download**

> > The total amount of bytes that were downloaded.

> **speed\_download**

> > The average download speed that curl measured for the complete download.

> **speed\_upload**

> > The average upload speed that curl measured for the complete upload.

> **header\_size**

> > The total size of all the headers received.

> **request\_size**

> > The total size of the issued requests.

> **content\_length\_download**

> > The content-length of the download.

> **content\_length\_upload**

> > The specified size of the upload.

> **starttransfer\_time**

> > Time it took from the start until the first byte was received.

> **redirect\_time**

> > Time it took for all redirection steps include name lookup, connect,
> > pre-transfer and transfer before final transaction was started.

> **redirect\_count**

> > The total number of redirections that were actually followed.

> **num\_connects**

> > The number of new connections that were created to achieve the transfer.

> **appconnect\_time**

> > Time it took from the start until the SSL connect/handshake to the remote
> > host was completed.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
