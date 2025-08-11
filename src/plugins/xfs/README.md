NCOLLECTD-XFS(5) - File Formats Manual

# NAME

**ncollectd-xfs** - Documentation of ncollectd's xfs plugin

# SYNOPSIS

	load-plugin xfs
	plugin xfs {
	    device [incl|include|excl|exclude] device
	    collect flags
	}

# DESCRIPTION

The **xfs** plugin collect metrics from xfs filesystems.

The following configuration options are available:

**device** \[*incl|include|excl|exclude*] *device*

> Select the filesystem based on the devicename.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**collect** *flags*

> *extentalloc*

> *abt*

> *blkmap*

> *bmbt*

> *dir*

> *trans*

> *ig*

> *log*

> *pushail*

> *sstrat*

> *rw*

> *attr*

> *icluster*

> *vnodes*

> *buf*

> *abtb2*

> *abtc2*

> *bmbt2*

> *ibt2*

> *fibt2*

> *rmapbt*

> *refcntbt*

> *qm*

> *xpc*

> *defer\_relog*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
