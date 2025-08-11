NCOLLECTD-WRITE\_MONGODB(5) - File Formats Manual

# NAME

**ncollectd-write\_mongodb** - Documentation of ncollectd's write\_mongodb plugin

# SYNOPSIS

	load-plugin write_mongodb
	plugin write_mongodb {
	    instance instance {
	        host host
	        port port
	        database database
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        store-rates true|false
	        metric-collection collection-name
	        metric-timestamp-field timestamp-field-name
	        metric-metadata-field metadata-field-name
	        metric-value-field value-field-name
	        notification-collection collection-name
	        notification-name-field name-field-name
	        notification-severity-field severity-field-name
	        notification-timestamp-field timestamp-field-name
	        notification-labels-field labels-field-name
	        notification-annotations-field annotations-field-name
	        write metrics|notifications
	        ttl seconds
	        bulk-size size
	    }
	}

# DESCRIPTION

The **write\_mongodb** plugin will insert metrics in a *MongoDB*,
a schema-less NoSQL database.

Within each **instance** block, the following options are available:

**host** *host*

> Hostname or address to connect to.
> Defaults to localhost.

**port** *port*

> Service name or port number to connect to.
> Defaults to 27017.

**database** *database*

**user** *user*

**user-env** *env-name*

**password** *password*

> Sets the information used when authenticating to a *MongoDB* database.
> The fields are optional (in which case no authentication is attempted),
> but if you want to use authentication all three fields must be set.

**password-env** *env-name*

> Get the password fox authenticating to a *MongoDB* database from the
> environment variable *env-name*.

**store-rates** *true|false*

**metric-collection** *collection-name*

> The collection name to write the metrics.
> The default name is "metrics".

**metric-timestamp-field** *timestamp-field-name*

> The name of the field which contains the date in each time series document.
> The default name is "timestamp".

**metric-metadata-field** *metadata-field-name*

> The name of the field which contains the labels in each time series document.
> The default name is "metadata".

**metric-value-field** *value-field-name*

> The name of the field witch contains the value in each time series document.
> The default name is "value".

**notification-collection** *collection-name*

> The collection name to write the notifications.
> The default name is "notifications".

**notification-name-field** *name-field-name*

> The name of the field which contains the notification name.
> The default name is "name".

**notification-severity-field** *severity-field-name*

> The name of the field which contains the notification severity.
> The default name is "severity".

**notification-timestamp-field** *timestamp-field-name*

> The name of the field which contains the date of the notification.
> The default name is "timestamp".

**notification-labels-field** *labels-field-name*

> The name of the field which contains the labels of the notification.
> The default name is "labels".

**notification-annotations-field** *annotations-field-name*

> The name of the field which contains the annotations of the notification.
> The default name is "annotations".

**write** *metrics|notifications*

> If set to *metrics* (the default) the plugin will handle metrics.
> If set to *notifications* the plugin will handle notifications.

**ttl** *seconds*

> Automatic removal of metrics older than a specified number of seconds.

**bulk-size** *size*

> The number of inserts in the bulk operation.
> The default number is 512.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
