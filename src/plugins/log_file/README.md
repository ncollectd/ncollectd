NCOLLECTD-LOG\_FILE(5) - File Formats Manual

# NAME

**ncollectd-log\_file** - Documentation of ncollectd's log\_file plugin

# SYNOPSIS

	load-plugin log_file
	plugin log_file {
	    log-level debug|info|notice|warning|err
	    file /path/to/file
	    format text|logfmt|json|logstash [[!]all|timestamp|severity|plugin|file|line|function|message ...]
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
> If the file does not exist it is created with mode 0640 (subject to the umask).

**format** *text|logfmt|json|logstash* \[\[!]*all|timestamp|severity|plugin|file|line|function|message* ...]

> Selects the format used to write the log messages: **text**, **logfmt**,
> **json** or **logstash**.
> Defaults to **text**.
> The optional arguments select which fields are written: **timestamp**,
> **severity**, **plugin**, **file**, **line**, **function** and **message**,
> or **all** of them. Prefix a field with **!** to remove it.
> The arguments are applied in order; by default all the fields are written.
> For example, `format text !file !line !function` writes the timestamp,
> the severity, the plugin name and the message.

There is no need to notify the daemon after moving or removing the
log file (e. g. when rotating the logs).
The plugin reopens the file for each line it writes.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

