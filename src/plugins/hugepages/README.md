NCOLLECTD-HUGEPAGES(5) - File Formats Manual

# NAME

**ncollectd-hugepages** - Documentation of ncollectd's hugepages plugin

# SYNOPSIS

	load-plugin hugepages
	plugin hugepages {
	    report-per-node-hp true|false
	    report-root-hp true|false
	    values-pages true|false
	    values-bytes true|false
	}

# DESCRIPTION

The **hugepages** plugin collect *hugepages* information, ncollectd
reads directories */sys/devices/system/node/\*/hugepages* and
*/sys/kernel/mm/hugepages*.
Reading of these directories can be disabled by the following
options (default is enabled).

**report-per-node-hp** *true|false*

> If enabled, information will be collected from the hugepage
> counters in */sys/devices/system/node/\*/hugepages*.
> This is used to check the per-node hugepage statistics on
> a NUMA system.

**report-root-hp** *true|false*

> If enabled, information will be collected from the hugepage
> counters in */sys/kernel/mm/hugepages*.
> This can be used on both NUMA and non-NUMA systems to check
> the overall hugepage statistics.

**values-pages** *true|false*

> Whether to report hugepages metrics in number of pages.
> Defaults to **true**.

**values-bytes** *true|false*

> Whether to report hugepages metrics in bytes.
> Defaults to **false**.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

