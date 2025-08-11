NCOLLECTD-LOGIND(5) - File Formats Manual

# NAME

**ncollectd-logind** - Documentation of ncollectd's logind plugin

# SYNOPSIS

	load-plugin logind
	plugin logind {
	    group-by [seat|remote|type|class] ...
	}

# DESCRIPTION

The **logind** plugin collects information from systemd-login logins.

The following configuration options are available:

**group-by** *\[seat|remote|type|class] ...*

> Select how login grouping is performed.
> Multiple criteria can be specified.
> If the string *All* is used, al criterias are selected.
> If a criterion begins with "!" it is deselected.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
