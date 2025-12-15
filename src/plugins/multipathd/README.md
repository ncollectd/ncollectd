UNTITLED - LOCAL

# NAME

**ncollectd-zswap** - Documentation of ncollectd's multipathd plugin

# SYNOPSIS

	load-plugin multipathd
	plugin multipathd {
	    timeout seconds
	}

# DESCRIPTION

The **multipathd** plugin collect statistics related to multipath devices from multipathd.  
The **timeout** option sets the timeout for request to the multipathd daemon to show the paths.
**URL**, in seconds.
By default, the configured **interval** is used to set the timeout.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

