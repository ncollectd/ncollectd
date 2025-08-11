NCOLLECTD-JOURNAL(5) - File Formats Manual

# NAME

**ncollectd-journal** - Documentation of ncollectd's journal plugin

# SYNOPSIS

	load-plugin journal
	plugin journal {
	    instance name {
	        namespace namespace
	        path path
	        unit name
	        interval seconds
	        match match {
	            ...
	        }
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **journal** plugin reads logs from the systemd journal and process
the lines using a match plugin.

The configuration consists of one or more **instance** blocks that define a
systemd journal reader and a match plugin to process the log lines.

The **instance** block has the following options:

**namespace** *namespace*

> Set the journal namespace from which to read journal messages.

**path** *path*

> Set the directory from which to read journal files.

**unit** *name*

> Get messages for the specified systemd unit.

**interval** *seconds*

> Configures the interval in which to read log lines.
> Defaults to the plugin's default interval.

**match** *match*

> Configure the *match* plugin to process the log lines.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
