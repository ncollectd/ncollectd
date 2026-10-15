NCOLLECTD-XFS(5) - File Formats Manual

# NAME

**ncollectd-xfs** - Documentation of ncollectd's xfs plugin

# SYNOPSIS

	load-plugin xfs
	plugin xfs {
	    device [incl|include|excl|exclude] device
	    collect flags
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **xfs** plugin collect metrics from xfs filesystems.

The following configuration options are available:

**device** \[*incl|include|excl|exclude*] *device*

> Select the filesystem based on the devicename.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**collect** *flags*

> *ExtentAlloc*

> *Abt*

> *BlkMap*

> *Bmbt*

> *Dir*

> *Trans*

> *IG*

> *Log*

> *PushAil*

> *XStrat*

> *RW*

> *Attr*

> *ICluster*

> *VNodes*

> *Buf*

> *Abtb2*

> *Abtc2*

> *Bmbt2*

> *Ibt2*

> *Fibt2*

> *rMapBt*

> *RefCntBt*

> *rMapBtMem*

> *RcBagBt*

> *RtrMapBt*

> *RtrMapBtMem*

> *RtRedCntBt*

> *Qm*

> *Zoned*

> *MetaFile*

> *Xpc*

> *defer\_relog*

> *GcXpc*

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

