NCOLLECTD-POSTFIX(5) - File Formats Manual

# NAME

**ncollectd-postfix** - Documentation of ncollectd's postfix plugin

# SYNOPSIS

	load-plugin postfix
	plugin postfix {
	    instance name {
	        unit unit-name
	        log-path path-to-log
	        log-from file|systemd
	        showq-path path-to-showq-socket
	        histogram-time-buckets [bucket] ...
	        histogram-queue-size-buckets [bucket] ...
	        histogram-queue-age-buckets [bucket] ...
	        histogram-qmgr-inserts-nrcpt-buckets [bucket] ...
	        histogram-qmgr-inserts-size-buckets [bucket] ...
	        timeout seconds
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **postfix** plugin will collect statistics from a *postfix* server
reading the logs and access the showq socket to list the mail queue.

The plugin configuration consists of one or more **instance** blocks which
specify one *postfix* instance.
Within the **instance** blocks, the following options are allowed:

**unit** *unit-name*

> Set the unit name from which to read the logs stored in the journal.
> The default value is postfix.service.

**log-path** *path-to-log*

> Set file path from which to read the logs.
> The default value is /var/log/mail.log.

**log-from** *file|systemd*

> Define from with source to read the postfix logs.
> By default if systemd is detected the logs will be read from the journal.

**showq-path** *path-to-showq-socket*

> Set the path to the showq postfix socket.
> The default value is /var/spool/postfix/public/showq.

**histogram-time-buckets** \[*bucket*] ...

> Define the list of buckets for time based histograms.

**histogram-queue-size-buckets** \[*bucket*] ...

> Define the list of buckets for queue size histogram.

**histogram-queue-age-buckets** \[*bucket*] ...

> Define the list of buckets for queue age histogram.

**histogram-qmgr-inserts-nrcpt-buckets** \[*bucket*] ...

> Define the list of buckets for qmgr number of receipients per message histogram.

**histogram-qmgr-inserts-size-buckets** \[*bucket*] ...

> Define the list of buckets for qmgr inserts histogram.

**timeout** *seconds*

> Set the timeout in seconds for read the showq socket.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instance.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
