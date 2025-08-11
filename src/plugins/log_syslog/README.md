NCOLLECTD-LOG\_SYSLOG(5) - File Formats Manual

# NAME

**ncollectd-log\_syslog** - Documentation of ncollectd's log\_syslog plugin

# SYNOPSIS

	load-plugin log_syslog
	plugin log_syslog {
	    instance name {
	        host host
	        port port
	        log-level [debug|info|notice|warning|err]
	        notify-level [okay|warning|failure]
	        facility [kern|user|mail|daemon|auth|syslog|lpr|news|uucp|cron|authpriv|ftp|ntp|security|console|local[0-7]]
	        type [local|rfc3164|rfc5424]
	        ttl ttl
	    }
	}

# DESCRIPTION

The **log\_syslog** plugin receives log messages from the daemon and dispatches
them to a local
syslog(3)
or to a remote syslog.

The **log\_syslog** plugin can also act as a consumer of notifications
if the **notify-level** option is provided.

**host** *host*

**port** *port*

**log-level** \[*debug|info|notice|warning|err*]

> Sets the log-level.
> If, for example, set to *notice*, then all events with severity
> *notice*, *warning*, or *err* will be submitted to the
> syslog-daemon.

> Please note that *debug* is only available if ncollectd has been
> compiled with debugging support.

**notify-level** \[*okay|warning|failure*]

> Controls which notifications should be sent to syslog.
> The default behaviour is not to send any.
> Less severe notifications always imply logging more severe notifications:
> Setting this to *okay* means all notifications will be sent to syslog,
> setting this to *warning* will send *warning* and *failure*
> notifications but will dismiss *okay* notifications.
> Setting this option to *failure* will only send failures to syslog.

**facility** \[*kern|user|mail|daemon|auth|syslog|lpr|news|uucp|cron|authpriv|ftp|ntp|security|console|local\[0-7]*]

**type** \[*local|rfc3164|rfc5424*]

**ttl** *ttl*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
