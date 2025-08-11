NCOLLECTD-WRITE\_HTTP(5) - File Formats Manual

# NAME

**ncollectd-write\_http** - Documentation of ncollectd's write\_http plugin

# SYNOPSIS

	load-plugin write_http
	plugin write_http {
	    instance instance {
	        url url
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        verify-peer true|false
	        verify-host true|false
	        ca-cert /path/to/ca-cert
	        ca-path /path/to/ca/
	        client-key /path/to/client-key
	        client-cert /path/to/client-cert
	        client-key-pass key-pass
	        ssl-version default|sslv2|sslv3|tlsv1|tlsv1_0|tlsv1_1|tlsv1_2|tlsv1_3
	        compress none|snappy|gzip|zlib|deflate
	        collect flags
	        store-rates true|false
	        buffer-size size
	        low-speed-limit limit
	        timeout seconds
	        log-http-error true|false
	        header header
	        flush-interval seconds
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	        format-notification text|json|protob
	        write metrics|notifications
	    }
	}

# DESCRIPTION

The **write\_http plugin** submits values to an HTTP server using
POST requests.

The plugin can send values to multiple HTTP servers by specifying one
**instance** block for each server.
Within each **instance** block, the following options are available:

**url** *url*

> URL to which the values are submitted to.
> Mandatory.

**user** *username*

> Optional user name needed for authentication.

**user-env** *env-name*

> Get the user name for authentication from the enviroment variable
> *env-name*.

**password** *password*

> Optional password needed for authentication.

**password-env** *env-name*

> Get the password for authentication from the enviroment variable
> *env-name*.

**verify-peer** *true|false*

> Enable or disable peer SSL certificate verification.
> Enabled by default.

**verify-host** *true|false*

> Enable or disable peer host name verification.
> If enabled, the plugin checks if the Common Name or a
> Subject Alternate Name field of the SSL certificate matches the
> host name provided by the **url** option.
> If this identity check fails, the connection is aborted.
> Obviously, only works when connecting to a SSL enabled server.
> Enabled by default.

**ca-cert** */path/to/ca-cert*

> File that holds one or more SSL certificates.
> If you want to use HTTPS you will possibly need this option.
> What CA certificates come in your system and are checked by default
> depends on the distribution you use.

**ca-path** */path/to/ca/*

> Directory holding one or more CA certificate files.
> You can use this if for some reason all the needed CA certificates aren't
> in the same file and can't be pointed to using the **ca-cert** option.

**client-key** */path/to/client-key*

> File that holds the private key in PEM format to be used for certificate-based
> authentication.

**client-cert** */path/to/client-cert*

> File that holds the SSL certificate to be used for certificate-based
> authentication.

**client-key-pass** *key-pass*

> Password required to load the private key in **client-key**.

**ssl-version** *default|sslv2|sslv3|tlsv1|tlsv1\_0|tlsv1\_1|tlsv1\_2|tlsv1\_3*

> Define which SSL protocol version must be used.

**compress** *none|snappy|gzip|zlib|deflate*

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

**store-rates** *true|false*

> If set to **true**, convert counter values to rates.
> If set to **false** (the default) counter values are stored as is,
> i.e. as an increasing integer number.

**buffer-size** *size*

> Sets the send buffer size to *bytes*.
> By increasing this buffer, less HTTP requests will be generated, but more
> metrics will be batched / metrics are cached for longer before being sent,
> introducing additional delay until they are available on the server side.
> *Bytes* must be at least 1024 and cannot exceed the size of an
> int, i.e. 2 GByte.
> Defaults to 4096.

**low-speed-limit** *limit*

> Sets the minimal transfer rate in I&lt;Bytes per Second&gt; below which the
> connection with the HTTP server will be considered too slow and aborted.
> All the data submitted over this connection will probably be lost.
> Defaults to 0, which means no minimum transfer rate is enforced.

**timeout** *seconds*

> Sets the maximum time in milliseconds given for HTTP POST operations to
> complete.
> When this limit is reached, the POST operation will be aborted, and all
> the data in the current send buffer will probably be lost.
> Defaults to 0, which means the connection never times out.

**log-http-error** *true|false*

> Enables printing of HTTP error code to log.
> Turned off by default.

**header** *header*

> A HTTP header to add to the request.
> Multiple headers are added if this option is specified more than once.
> Example:

> >     header "X-Custom-Header: custom\_value"

**flush-interval** *seconds*

**format-metric** *influxdb|graphite|json|kairosdb|opentsdb|openmetrics|remote*

> Selects the format in which metrics are written.

> **influxdb** \[*sec|msec|usec|nsec*]

> > Format the metrics with the influxdb line format.

> **graphite**

> > Format the metrics with the graphite plaintext format.

> **json**

> > Format the metrics as json message.

> **kairosdb** \[*telnet \[sec|msec]|json*]

> > Format the metrics with the karirosdb text format or json format.

> **opentsdb** \[telnet|json]

> > Format the metrics with the opentsdb text format or json format.

> **openmetrics** \[text|protob]

> > Format the metrics with the openmetrics text format.

> **opentelemetry** \[json]

> > Format the metrics with the opentelemetry json format.

> **remote** \[metadata]

> > Format the metrics with the protocol buffer remote format.

**format-notification** *text|json|protob*

> Selects the format in which notifications are written.

**write** *metrics|notifications*

> If set to *metrics* (the default) the plugin will handle metrics.
> If set to *notifications* the plugin will handle notifications.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
