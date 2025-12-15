NCOLLECTD-LOG\_SYSTEMD(5) - File Formats Manual

# NAME

**ncollectd-log\_systemd** - Documentation of ncollectd's log\_systemd plugin

# SYNOPSIS

	load-plugin log_systemd
	plugin log_systemd {
	    log-level debug|info|notice|warning|err
	    notify-level okay|warning|failure
	}

# DESCRIPTION

The **log\_systemd** plugin receives log messages from the daemon and send them
to the
systemd(1)
journal.

The plugin supports the following options:

**log-level** *debug|info|notice|warning|err*

> Sets the log-level.
> If, for example, set to *notice*, then all events with severity
> *notice*, *warning*, *err* will be submitted to the
> systemd journal.

> Please note that *debug* is only available if collectd has been compiled
> with debugging support.

**notify-level** *okay|warning|failure*

> Controls which notifications should be sent to the system journal.
> The default behaviour is not to send any.
> Less severe notifications always imply logging more severe notifications:
> Setting this to **okay** means all notifications will be sent to systemd,
> setting this to **warning** will send **warning** and **failure**
> notifications but will dismiss **okay** notifications.
> Setting this option to **failure** will only send failures to systemd.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

