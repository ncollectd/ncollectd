NCOLLECTD-FCHOST(5) - File Formats Manual

# NAME

**ncollectd-fchost** - Documentation of ncollectd's fchost plugin

# SYNOPSIS

	load-plugin fchost
	plugin fchost {
	    host [incl|include|excl|exclude] host
	}

# DESCRIPTION

The **fchost** plugin collectd information about HBA/Fibre Channel used
in the system, collecting data from files in path */sys/class/fc\_host*.

The plugin support the following options:

**host** \[*incl|include|excl|exclude*] *host*

> Select the *host* (like *host0*) to collectd data from.
> If no **host** option is configured, all hosts are collected.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

