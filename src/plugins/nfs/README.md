NCOLLECTD-NFS(5) - File Formats Manual

# NAME

**ncollectd-nfs** - Documentation of ncollectd's nfs plugin

# SYNOPSIS

	load-plugin nfs
	plugin nfs {
	    collect nfs-v2|nfs-v3|nfs-v4|mount-stats
	}

# DESCRIPTION

The **nfs** plugin collects information about the client usage of the Network
File System (NFS). It counts the number of procedure calls for each procedure,
grouped by version and collects the statistics per mount point.

The **nfs** plugin the the following option:

**collect** *nfs-v2|nfs-v3|nfs-v4|mount-stats*

> The option *nfs-v2* collect NFSv2 procedure call statistics, *nfs-v3*
> collect NFSv3 procedure call statistics and *nfs-v4* collect NFSv4
> procedure call statistics.
> The option *mount-stats* collect statistics of NFS mounted filesystems.
> With *all* you can enable all options, and with *!all* you can
> disable all.
> Putting up **!** in front of an option you disable that option,
> for example: *!nfs-v2*.
> You can put multiple options in one line for example:

> >   **collect** *"!all"* *nfs-v4*

> You can disable or enable multiple options with glob:

> >   **collect** *"!nfs-\*"* *mount-stats*

> By default all options are enabled.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
