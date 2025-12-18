NCOLLECTD-NFSD(5) - File Formats Manual

# NAME

**ncollectd-nfsd** - Documentation of ncollectd's nfsd plugin

# SYNOPSIS

	load-plugin nfsd
	plugin nfsd {
	    collect nfs-v2|nfs-v3|nfs-v4
	}

# DESCRIPTION

The **nfsd** plugin collects information about the server statistics
of the Network File System (NFS).
It counts the number of procedure calls for each procedure,
grouped by version.

**collect** *nfs-v2|nfs-v3|nfs-v4*

> The option *nfs-v2* collect NFSv2 procedure call statistics, *nfs-v3*
> collect NFSv3 procedure call statistics and *nfs-v4* collect NFSv4
> procedure call statistics.
> Putting up **!** in front of an option you disable that option,
> for example: *!nfs-v2*.
> You can put multiple options in one line for example:

> >   **collect** *"!all"* *nfs-v4*

> By default all options are enabled.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

