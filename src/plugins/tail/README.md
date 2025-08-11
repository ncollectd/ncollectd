NCOLLECTD-TAIL(5) - File Formats Manual

# NAME

**ncollectd-tail** - Documentation of ncollectd's tail plugin

# SYNOPSIS

	load-plugin tail
	plugin tail {
	    file filename {
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

The **tail** plugin reads files and process the lines using a match plugin.

The configuration consists of one or more **file** blocks that define a
**filename** to read and a match plugin to process the lines.
The **file** block has the following options:

**interval** *seconds*

> Configures the interval in which to read lines from this file.
> Defaults to the plugin's default interval.

**match** *match*

> Configure the *match* plugin to process the lines of the file.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
