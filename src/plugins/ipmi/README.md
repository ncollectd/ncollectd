NCOLLECTD-IPMI(5) - File Formats Manual

# NAME

**ncollectd-ipmi** - Documentation of ncollectd's ipmi plugin

# SYNOPSIS

	load-plugin ipmi
	plugin ipmi {
	    instance instance {
	        address address
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        auth-type MD5|rmcp+
	        host hostname
	        label key value
	        interval seconds
	        sensor true|false
	        notify-ipmi-connection-state true|false
	        notify-sensor-add true|false
	        notify-sensor-remove true|false
	        notify-sensor-not-present true|false
	        sel-sensor sensor
	        sel-enable true|false
	        sel-celear-event true|false
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ipmi** plugin allows to monitor server platform status using the
Intelligent Platform Management Interface (IPMI).
Local and remote interfaces are supported.

The plugin configuration consists of one or more **instance** blocks which
specify one *ipmi* connection each.
Each block requires one unique string argument as the instance name.
If instances are not configured, an instance with the default option values
will be created.

Within the **instance** blocks, the following options are allowed:

**address** *address*

> Hostname or IP to connect to.
> If not specified, plugin will try to connect to local management
> controller (BMC).

**user** *username*

**user-env** *env-name*

**password** *password*

> The username and the password to use for the connection to remote BMC.

**password-env** *env-name*

> Get the username and the password to use for the connection to remote
> BMC from the environment variable *env-name*.

**auth-type** *MD5|rmcp+*

> Forces the authentication type to use for the connection to remote BMC.
> By default most secure type is selected.

**host** *hostname*

> Sets the **hostname** label on dispatched values and notifications.
> Defaults to the global hostname setting.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple times.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.

**sensor** *sensor*

> Selects sensors to collect or to ignore.

**notify-ipmi-connection-state** *true|false*

> If a IPMI connection state changes after initialization time of a minute
> a notification is sent.
> Defaults to *false*.

**notify-sensor-add** *true|false*

> If a sensor appears after initialization time of a minute a notification
> is sent.

**notify-sensor-remove** *true|false*

> If a sensor disappears a notification is sent.

**notify-sensor-not-present** *true|false*

> If you have for example dual power supply and one of them is (un)plugged
> then a notification is sent.

**sel-sensor** *sensor*

> Selects sensors to get events from or to ignore.

**sel-enable** *true|false*

> If system event log (SEL) is enabled, plugin will listen for sensor threshold
> and discrete events.
> When event is received the notification is sent.
> SEL event filtering can be configured using **sel-sensor**.
> Defaults to *false*.

**sel-celear-event** *true|false*

> If SEL clear event is enabled, plugin will delete event from SEL list after
> it is received and successfully handled.
> In this case other tools that are subscribed for SEL events will receive an
> empty event.
> Defaults to *false*.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

