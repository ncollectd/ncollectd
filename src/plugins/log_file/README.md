NCOLLECTD-LOG\_FILE(5) - File Formats Manual

# NAME

**ncollectd-log\_file** - Documentation of ncollectd's log\_file plugin

# SYNOPSIS

	load-plugin log_file
	plugin log_file {
	    log-level debug|info|notice|warning|err
	    file /path/to/file
	    print-timestamp true|false
	    print-severity true|false
	}

# DESCRIPTION

The **log\_file** plugin receives log messages from the daemon and
writes them to a text file.

In order for other plugins to be able to report errors and warnings
during initialization, the **log\_file** plugin should be loaded as
one of the first plugins, if not as the first plugin.
This means that its **load-plugin** line should be one of the first
lines in the configuration file.

The plugin has de following options:

**log-level** *debug|info|notice|warning|err*

> Sets the log-level.
> If, for example, set to **notice**, then all events with
> severity **notice**, **warning**, or **err** will
> be written to the logfile.

> Please note that **debug** is only available if ncollectd
> has been compiled with debugging support.

**file** */path/to/file*

> Sets the file to write log messages to.
> The special strings **stdout** and **stderr** can be used to write to
> the standard output and standard error channels, respectively.
> This, of course, only makes much sense when ncollectd is running in
> foreground or non-daemon-mode.

**print-timestamp** *true|false*

> Prefix all lines printed by the current time.
> Defaults to **true**.

**print-severity** *true|false*

> When enabled, all lines are prefixed by the severity of the log message, for
> example "warning".
> Defaults to **false**.

There is no need to notify the daemon after moving or removing the
log file (e. g. when rotating the logs).
The plugin reopens the file for each line it writes.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
