NCOLLECTD-CGROUPS(5) - File Formats Manual

# NAME

**ncollectd-cgroups** - Documentation of ncollectd's cgroups plugin

# SYNOPSIS

	load-plugin cgroups
	plugin cgroups {
	    cgroup [incl|include|excl|exclude] cgroup
	    filter {
	        ...
	    }
	}

# DESCRIPTION

The **cgroups** plugin collects CPU, IO and memory metrics from
the groups configured in Control Groups V1 or Control Groups V2.

The plugin has the following options:

**cgroup** \[*incl|include|excl|exclude*] *cgroup*

> Select *cgroup* based on the name.
> By default only selected cgroups are collected if a selection is made.
> If no selection is configured at all, **all** cgroups are selected.
> The root cgroup is matched with the name */*.
> The selection does not stop the plugin from descending into the children
> of a non selected cgroup: they are matched by their own name.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

