NCOLLECTD-WRITE\_AMQP1(5) - File Formats Manual

# NAME

**ncollectd-write\_amqp1** - Documentation of ncollectd's write\_amqp1 plugin

# SYNOPSIS

	load-plugin write_amqp1
	plugin write_amqp1 {
	    instance name {
	        host host
	        port port
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        address address
	        retry-delay retry-delay
	        format FORMAT
	        pre-settle true|false
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	        format-notification text|json|protob
	        write metrics|notifications
	    }
	}

# DESCRIPTION

The **write\_amqp1** plugin can be used to send metrics or notifications
using an AMQP 1.0 message intermediary.

The following options are accepted within each **instance** block:

**host** *host*

> Hostname or IP-address of the AMQP 1.0 intermediary.
> Defaults to the default behavior of the underlying communications library,
> *libqpid-proton*, which is "localhost".

**port** *port*

> Service name or port number on which the AMQP 1.0 intermediary accepts
> connections.
> This argument must be a string, even if the numeric form is used.
> Defaults to "5672".

**user** *user*

**user-env** *env-name*

**password** *password*

> Credentials used to authenticate to the AMQP 1.0 intermediary.
> By default "guest"/"guest" is used.

**password-env** *env-name*

> Get the credentials used to authenticate to the AMQP 1.0 intermediary from
> the environment variable *env-name*.

**address** *address*

> This option specifies the prefix for the send-to value in the message.

**retry-delay** *retry-delay*

> When the AMQP1 connection is lost, defines the time in seconds to wait
> before attempting to reconnect.
> Defaults to 1, which implies attempt to reconnect at 1 second intervals.

**pre-settle** *true|false*

> If set to *false* (the default), the plugin will wait for a message
> acknowledgement from the messaging bus before sending the next
> message.
> This indicates transfer of ownership to the messaging system.
> If set to *true*, the plugin will not wait for a message
> acknowledgement and the message may be dropped prior to transfer of
> ownership.

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

