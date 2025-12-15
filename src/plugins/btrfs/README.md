NCOLLECTD-BTRFS(5) - File Formats Manual

# NAME

**ncollectd-btrfs** - Documentation of ncollectd's btrfs plugin

# SYNOPSIS

	load-plugin brtfs
	plugin brtfs {
	    refresh-mounts true|false
	}

# DESCRIPTION

The **btrfs** plugin reports statistics of mounted btrfs filesystems.

The plugin has the following options:

**refresh-mounts** *true|false*

> Not caching the mount points, refresh in every read.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

