NCOLLECTD-MQ(5) - File Formats Manual

# NAME

**ncollectd-mq** - Documentation of ncollectd's mq plugin

# SYNOPSIS

	load-plugin mq
	plugin mq {
	    instance name {
	        host hostname
	        port port
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        queue-manager manager
	        connection-channel channel
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **mq** plugin collect metrics from a MQ server.

The **instance** block support the following options.

**host** *hostname*

> Hostname of the MQ server to connect to.

**port** *port*

> Port number or service name of the MQ server to connect to.

**user** *username*

> Username used when authenticating to the MQ server.

**user-env** *env-name*

> Get the username used when authenticating to the MQ server from the
> environment variable *env-name*

**password** *password*

> Password used when authenticating to the MQ server.

**password-env** *env-name*

> Get the password used when authenticating to the MQ server from the
> environment variable *env-name*

**queue-manager** *manager*

> Sets the *queue-manger* name to connect.

**connection-channel** *channel*

> Sets the *client connection channel* to connect.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected
> from this instance.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

