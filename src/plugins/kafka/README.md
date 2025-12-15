NCOLLECTD-KAFKA(5) - File Formats Manual

# NAME

**ncollectd-kafka** - Documentation of ncollectd's kafka plugin

# SYNOPSIS

	load-plugin kafka
	plugin kafka {
	    instance instance {
	        property key value
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **kafka** plugin collect metrics from a kafka cluster.

The plugin support the following options:

**property** *key* *value*

> Configure the kafka client through properties, you almost always will
> want to set **metadata.broker.list** to your Kafka broker list.

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

