NCOLLECTD-CPU(5) - File Formats Manual

# NAME

**ncollectd-cpu** - Documentation of ncollectd's cpu plugin

# SYNOPSIS

	load-plugin cpu
	plugin cpu {
	    report-guest-state false|true
	    subtract-guest-state false|true
	    report-topology false|true
	}

# DESCRIPTION

The **cpu** plugin collects CPU usage metrics.

The following configuration options are available:

**report-guest-state** *false|true*

> When set to **true**, reports the &#8220;guest&#8221; and &#8220;guest\_nice&#8221;
> CPU states.
> Defaults to **false**.

**subtract-guest-state** *false|true*

> This option is only considered when **report-guest-state** is set to
> **true**.
> &#8220;guest&#8221; and &#8220;guest\_nice&#8221; are included in respectively
> &#8220;user&#8221; and &#8220;nice&#8221;.
> If set to **true**, &#8220;guest&#8221; will be subtracted from &#8220;user&#8221; and
> &#8220;guest\_nice&#8221; will be subtracted from &#8220;nice&#8221;.
> Defaults to **true**.

**report-topology** *false|true*

> When set to **true** in Linux, reports the cpu topology as labels:
> *node*, *code*, *socket*, *book* and *drawer*.
> Defaults to **false**.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

