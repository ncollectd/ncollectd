NCOLLECTD-CGROUPS(5) - File Formats Manual

# NAME

**ncollectd-cgroups** - Documentation of ncollectd's cgroups plugin

# SYNOPSIS

	load-plugin cgroups
	plugin cgroups {
	    cgroup [incl|include|excl|exclude] cgroup
	}

# DESCRIPTION

The **cgroups** plugin collects the CPU user/system time for each
*cgroup* by reading the *cpuacct.stat* files in the first
cpuacct-mountpoint (typically */sys/fs/cgroup/cpu.cpuacct* on
machines using systemd).

The plugin has the following options:

**cgroup** \[*incl|include|excl|exclude*] *cgroup*

> Select *cgroup* based on the name.
> By default only selected cgroups are collected if a selection is made.
> If no selection is configured at all, **all** cgroups are selected.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
