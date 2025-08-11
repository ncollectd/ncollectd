NCOLLECTD-WRITE\_MQTT(5) - File Formats Manual

# NAME

**ncollectd-write\_mqtt** - Documentation of ncollectd's write\_mqtt plugin

# SYNOPSIS

	load-plugin write_mqtt
	plugin write_mqtt {
	    instance name {
	        host hostname
	        port port
	        topic topic
	        client-id client-id
	        user user
	        password user
	        qos 0|1|2
	        store-rates true|flase
	        retain true|flase
	        ca-cert file
	        certificate-file file
	        certificate-key-file file
	        tls-protocol protocol
	        cipher-suite ciper-suite
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	        format-notification text|json|protob
	        write metrics|notifications
	    }
	}

# DESCRIPTION

The **write\_mqtt** plugin can send metrics or notifications to a MQTT Broker.

Within each **instance** block, the following options are available:

**host** *hostname*

> Hostname of the MQTT broker to connect to.

**port** *port*

> Port number or service name of the MQTT broker to connect to.

**topic** *topic*

> Configures the topic to publish the messages.

**client-id** *client-id*

> MQTT client ID to use.
> Defaults to the hostname used by **ncollectd**.

**user** *user*

> Username used when authenticating to the MQTT broker.

**user-env** *env-name*

> Get the username used when authenticating to the MQTT broker from
> the environment variable
> *env-name*.

**password** *user*

> Password used when authenticating to the MQTT broker.

**password-env** *env-name*

> Get the password used when authenticating to the MQTT broker from
> the environment variable
> *env-name*.

**qos** *0|1|2*

> Sets the *Quality of Service*, with the values *0*, *1*
> and *2* meaning:

> **0**

> > At most once

> **1**

> > At least once

> **2**

> > Exactly once

**store-rates** *true|flase*

**retain** *true|flase*

> Controls whether the MQTT broker will retain (keep a copy of) the last message
> sent to each topic and deliver it to new subscribers.
> Defaults to *false*.

**ca-cert** *file*

> Path to the PEM-encoded CA certificate file.
> Setting this option enables TLS communication with the MQTT broker, and as such,
> **port** should be the TLS-enabled port of the MQTT broker.
> This option enables the use of TLS.

**certificate-file** *file*

> Path to the PEM-encoded certificate file to use as client certificate when
> connecting to the MQTT broker.
> Only valid if **ca-cert** and **certificate-key-file** are also set.

**certificate-key-file** *file*

> Path to the unencrypted PEM-encoded key file corresponding to
> **certificate-file**.
> Only valid if **ca-cert** and **certificate-file** are also set.

**tls-protocol** *protocol*

> If configured, this specifies the string protocol version (e.g. tlsv1,
> tlsv1.2) to use for the TLS connection to the broker.
> If not set a default version is used which depends on the version of OpenSSL
> the Mosquitto library was linked against.
> Only valid if **ca-cert** is set.

**cipher-suite** *ciper-suite*

> A string describing the ciphers available for use.
> See
> ciphers(1)
> and the
> openssl ciphers utility for more information.
> If unset, the default ciphers will be used.
> Only valid if **ca-cert** is set.

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
