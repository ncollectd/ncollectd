NCOLLECTD-TAPE(5) - File Formats Manual

# NAME

**ncollectd-tape** - Documentation of ncollectd's tape plugin

# SYNOPSIS

	load-plugin tape
	plugin tape {
	    tape [incl|include|excl|exclude] name
	}

# DESCRIPTION

The **tape** plugin collects information about the usage of tapes.

**tape** \[*incl|include|excl|exclude*] *name*

> Select tape based on the devicename.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
