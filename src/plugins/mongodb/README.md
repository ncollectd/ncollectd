NCOLLECTD-MONGODB(5) - File Formats Manual

# NAME

**ncollectd-mongodb** - Documentation of ncollectd's mongodb plugin

# SYNOPSIS

	load-plugin mongodb
	plugin mongodb {
	    instance instance {
	        host host
	        port port
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        prefer-secondary-query true|false
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **mongodb** plugin will collect stats from *MongoDB*,
a schema-less NoSQL database.

The following options are available:

**host** *host*

> Hostname or address to connect to.
> Defaults to localhost.

**port** *port*

> Service name or port number to connect to.
> Defaults to 27017.

**user** *user*

**user-env** *env-name*

**password** *password*

> Sets the information used when authenticating to a *MongoDB* database.
> The fields are optional (in which case no authentication is attempted),
> but if you want to use authentication all three fields must be set.

**password-env** *env-name*

> Get the password to authenticating to a *MongoDB* database from the
> environment variable *env-name*.

**prefer-secondary-query** *true|false*

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

