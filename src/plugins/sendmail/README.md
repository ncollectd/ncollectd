NCOLLECTD-SENDMAIL(5) - File Formats Manual

# NAME

**ncollectd-sendmail** - Documentation of ncollectd's sendmail plugin

# SYNOPSIS

	load-plugin sendmail
	plugin sendmail {
	    instance name {
	        config-path config-file-path
	        queue-path queue-path
	        histogram-queue-size-buckets [bucket] ...
	        histogram-queue-age-buckets [bucket] ...
	        label key value
	        interval interval
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **sendmail** plugin will collect statistics from a *sendmail* MTA
using stastus file.

The plugin configuration consists of one or more **instance** blocks which
specify one *sendmail* MTA each.
Within the **instance** blocks, the following options are allowed:

**config-path** *config-file-path*

> Set the sendmail config file path to get the mailers name and status file path.
> The default value is /etc/mail/sendmail.cf.

**queue-path** *queue-path*

> Set the path for de queue directory.
> The default value is /var/spool/mqueue.

**histogram-queue-size-buckets** \[*bucket*] ...

> Define the list of buckets for queue size histogram.

**histogram-queue-age-buckets** \[*bucket*] ...

> Define the list of buckets for queue age histogram.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *interval*

> Sets the interval (in seconds) in which the values will be collected from this
> instance.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in

ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
