NCOLLECTD-NOTIFY\_NAGIOS(5) - File Formats Manual

# NAME

**ncollectd-notify\_nagios** - Documentation of ncollectd's notify\_nagios plugin

# SYNOPSIS

	load-plugin notify_nagios
	plugin notify_nagios {
	    command-file path
	}

# DESCRIPTION

The **notify\_nagios** plugin writes notifications to Nagios'
*command file* as a *passive service check result*.

Available configuration options:

**command-file** *path*

> Sets the *command file* to write to.
> Defaults to /usr/local/nagios/var/rw/nagios.cmd.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

