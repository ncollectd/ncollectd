NCOLLECTD-MOSQUITTO(5) - File Formats Manual

# NAME

**ncollectd-mosquitto** - Documentation of ncollectd's mosquitto plugin

# SYNOPSIS

	load-plugin mosquitto
	plugin mosquitto {
	    instance name {
	        host hostname
	        port port
	        qos qos
	        client-id [0-2]
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        clean-session true|false
	        ca-cert /path/to/ca
	        certificate-file /path/to/cert
	        certificate-key-file /path/to/key
	        tls-protocol protocol
	        cipher-suite ciphersuite
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **mosquitto** plugin collect metrics from a Mosquitto server.

The following options are accepted within each **instance** block:

**host** *hostname*

> Hostname of the Mosquitto broker to connect to.

**port** *port*

> Port number or service name of the Mosquitto broker to connect to.

**qos** \[*0-2*]

> Sets the *Quality of Service*, with the values **0**, **1** and
> **2** meaning:

> **0**

> > At most once

> **1**

> > At least once

> **2**

> > Exactly once

**client-id** *ncollectd*

> MQTT client ID to use.
> Defaults to the hostname used by ncollectd.

**user** *username*

> Username used when authenticating to the Mosquitto broker.

**user-env** *env-name*

> Get the username used when authenticating to the Mosquitto broker from the
> environment variable *env-name*.

**password** *password*

> Password used when authenticating to the Mosquitto broker.

**password-env** *env-name*

> Get the password used when authenticating to the Mosquitto broker from the
> environment variable *env-name*.

**clean-session** *true|false*

> Controls whether the MQTT "cleans" the session up after the subscriber
> disconnects or if it maintains the subscriber's subscriptions and all messages
> that arrive while the subscriber is disconnected.
> Defaults to **true**.

**ca-cert** */path/to/ca*

> Path to the PEM-encoded CA certificate file.
> Setting this option enables TLS communication with the MQTT broker,
> and as such, *port* should be the TLS-enabled port of the MQTT broker.
> This option enables the use of TLS.

**certificate-file** */path/to/cert*

> Path to the PEM-encoded certificate file to use as client certificate when
> connecting to the MQTT broker.
> Only valid if **ca-cert** and **certificate-key-file** are also set.

**certificate-key-file** */path/to/key*

> Path to the unencrypted PEM-encoded key file corresponding
> to **certificate-file**
> Only valid if **ca-cert** and **certificate-file** are also set.

**tls-protocol** *protocol*

> If configured, this specifies the string protocol version (e.g. *tlsv1*,
> *tlsv1.2*) to use for the TLS connection to the broker.
> If not set a default version is used which depends on the version of OpenSSL
> the Mosquitto library was linked against.
> Only valid if **ca-cert** is set.

**cipher-suite** *ciphersuite*

> A string describing the ciphers available for use.
> See
> ciphers(1)
> and the *openssl ciphers* utility for more information.
> If unset, the default ciphers will be used.
> Only valid if **ca-cert** is set.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this Mosquitto broker.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
