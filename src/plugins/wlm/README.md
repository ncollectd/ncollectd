NCOLLECTD-WLM(5) - File Formats Manual

# NAME

**ncollectd-wlm** - Documentation of ncollectd's wlm plugin

# SYNOPSIS

	load-plugin wlm
	plugin wlm {
	    class [incl|include|excl|exclude] name
	}

# DESCRIPTION

The **wlm** plugin reads statistics from the *Workload Manager*.

The following configuration options are available:

**class** \[*incl|include|excl|exclude*] *name*

> Select workload manager class based on the class name.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

