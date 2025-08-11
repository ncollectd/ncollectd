NCOLLECTD-WRITE\_AMQP(5) - File Formats Manual

# NAME

**ncollectd-write\_amqp** - Documentation of ncollectd's write\_amqp plugin

# SYNOPSIS

	load-plugin write_amqp
	plugin write_amqp {
	    instance name {
	        host host [host ...]
	        port port
	        vhost vhost
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        tls-enabled true|false
	        tls-verify-peer true|false
	        tls-verify-hostname true|false
	        tls-ca-cert /proc/to/ca
	        tls-client-cert /proc/to/cert
	        tls-client-key /proc/to/key
	        exchange exchange
	        exchange-type type
	        routing-key key
	        persistent true|false
	        connection-retry-delay seconds
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	        format-notification text|json|protob
	        write metrics|notifications
	    }
	}

# DESCRIPTION

The **write\_amqp** plugin can be used to send metrics or notifications
to an AMQP 0.9.1 message broker.

The following options are accepted within each **instance** block:

**host** *host* \[*host* ...]

> Hostname or IP-address of the AMQP broker.
> Defaults to the default behavior of the underlying communications library,
> **rabbitmq-c**, which is "localhost".

> If multiple hosts are specified, then a random one is chosen at each
> (re)connection attempt.
> This is useful for failover with a clustered broker.

**port** *port*

> Service name or port number on which the AMQP broker accepts connections.
> This argument must be a string, even if the numeric form is used.
> Defaults to "5672".

**vhost** *vhost*

> Name of the *virtual host* on the AMQP broker to use.
> Defaults to "/".

**user** *username*

**user-env** *env-name*

**password** *password*

> Credentials used to authenticate to the AMQP broker.
> By default "guest"/"guest" is used.

**password-env** *env-name*

> Get the credentials used to authenticate to the AMQP broker from the
> environment variable *env-name*.

**tls-enabled** *true|false*

> If set to *true* then connect to the broker using a TLS connection.
> If set to *false* (the default), then a plain text connection is used.
> Requires rabbitmq-c &gt;= 0.4.

**tls-verify-peer** *true|false*

> If set to *true* (the default) then the server certificate
> chain is verified.
> Setting this to *false* will skip verification (insecure).
> Requires rabbitmq-c &gt;= 0.8.

**tls-verify-hostname** *true|false*

> If set to *true* (the default) then the server host name is verified.
> Setting this to *false* will skip verification (insecure).
> Requires rabbitmq-c &gt;= 0.8.

**tls-ca-cert** */proc/to/ca*

> Path to the CA cert file in PEM format.

**tls-client-cert** */proc/to/cert*

> Path to the client certificate in PEM format.
> If this is set, then **tls-client-key** must be set as well.

**tls-client-key** */proc/to/key*

> Path to the client key in PEM format.
> If this is set, then **tls-client-key** must be set as well.

**exchange** *exchange*

> This option specifies the *exchange* to send values to.
> By default, "amq.fanout" will be used.

**exchange-type** *type*

> If given, the plugin will try to create the configured *exchange*
> with this *type* after connecting.

**routing-key** *key*

> This configures the routing key to set on all outgoing messages.

**persistent** *true|false*

> Selects the *delivery method* to use.
> If set to **true**, the *persistent* mode will be used,
> i. e. delivery is guaranteed.
> If set to *false* (the default), the *transient* delivery mode will be
> used, i.e. messages may be lost due to high load, overflowing queues or similar
> issues.

**connection-retry-delay** *seconds*

> When the connection to the AMQP broker is lost, defines the time in seconds to
> wait before attempting to reconnect.
> Defaults to 0, which implies ncollectd will attempt to reconnect at each time
> metric is submited.

**format-metric** *influxdb|graphite|json|kairosdb|opentsdb|openmetrics|remote*

> Selects the format in which messages are sent to the intermediary.

> **influxdb** \[*sec|msec|usec|nsec*]

> **graphite**

> **json**

> **kairosdb** \[*telnet \[sec|msec]|json*]

> **opentsdb** \[telnet|json]

> **openmetrics** \[text|protob]

> **opentelemetry** \[json]

> **remote** \[metadata]

**format-notification** *text|json|protob*

> Selects the format in which notifications are sent to the intermediary.

**write** *metrics|notifications*

> If set to *metrics* (the default) the plugin will handle metrics.
> If set to *notifications* the plugin will handle notifications.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
