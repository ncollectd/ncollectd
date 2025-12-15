NCOLLECTD-WRITE\_KAFKA(5) - File Formats Manual

# NAME

**ncollectd-write\_kafka** - Documentation of ncollectd's write\_kafka plugin

# SYNOPSIS

	load-plugin write_kafka
	plugin write_kafka {
	    instance name {
	        topic topic
	        property key value
	        key key
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	        format-notification text|json|protob
	        write metrics|notifications
	    }
	}

# DESCRIPTION

The **write\_kafka** plugin send metric and notifications to a *Kafka*
topic, a distributed queue.

The plugin's configuration consists of one or more **instance** blocks.
Each block is given a unique *name* and specifies one kafka producer.

Inside the **instance** block, the following options are understood:

**topic** *topic*

> Set the topic to send messages.

**property** *key* *value*

> Configure the kafka producer through properties, you almost always will
> want to set **metadata.broker.list** to your Kafka broker list.

**key** *key*

> Use the specified string as a partitioning key for the topic.
> Kafka breaks topic into partitions and guarantees that for a given topology,
> the same consumer will be used for a specific key.
> The special (case insensitive) string **random** can be used to specify
> that an arbitrary partition should be used.

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

