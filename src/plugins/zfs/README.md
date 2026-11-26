NCOLLECTD-ZFS(5) - File Formats Manual

# NAME

**ncollectd-zfs** - Documentation of ncollectd's zfs plugin

# SYNOPSIS

	load-plugin zfs
	plugin zfs {
	    collect flags
	}

# DESCRIPTION

The **zfs** plugin collects information about the Adaptive Replacement
Cache (ARC) of the Zeta File-System (ZFS).
It reports information such as the cache size, the various hits and misses
and the transferred data.
Currently only Linux is supported.

**collect** *flags*

> Select which statistics to collect.
> A flag can be negated with a leading **!**, **all** selects all the flags.

> **abdstats**

> > ABD (ARC buffer data) statistics.

> **arcstats**

> > ARC statistics. Enabled by default.

> **dbufstats**

> > DMU buffer statistics.

> **dmu\_tx**

> > DMU transaction statistics.

> **dnodestats**

> > Dnode statistics.

> **fm**

> > Fault management statistics.

> **qat**

> > Intel QuickAssist Technology statistics.

> **vdev\_cache\_stats**

> > Vdev cache statistics.

> **vdev\_mirror\_stats**

> > Vdev mirror statistics.

> **xuio\_stats**

> > XUIO statistics.

> **zfetchstats**

> > Prefetch statistics.

> **zil**

> > ZFS Intent Log statistics.

> **state**

> > Pool state.

> **io**

> > Pool I/O statistics.

> **objset**

> > Dataset statistics.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

