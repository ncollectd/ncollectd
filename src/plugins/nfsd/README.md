NCOLLECTD-NFSD(5) - File Formats Manual

# NAME

**ncollectd-nfsd** - Documentation of ncollectd's nfsd plugin

# SYNOPSIS

	load-plugin nfsd
	plugin nfsd {
	    report-v2 true|false
	    report-v3 true|false
	    report-v4 true|false
	}

# DESCRIPTION

The **nfsd** plugin collects information about the server statistics
of the Network File System (NFS).
It counts the number of procedure calls for each procedure,
grouped by version.

**report-v2** *true|false*

> Collect NFS v2 procedure calls metrics.

**report-v3** *true|false*

> Collect NFS v3 procedure calls metrics.

**report-v4** *true|false*

> Collect NFS v4 procedure calls metrics.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

