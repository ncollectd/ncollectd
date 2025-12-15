NCOLLECTD-INTERFACE(5) - File Formats Manual

# NAME

**ncollectd-interface** - Documentation of ncollectd's interface plugin

# SYNOPSIS

	load-plugin interface
	plugin interface {
	    interface [incl|include|excl|exclude] interface
	    report-inactive true|false
	    unique-name true|false
	}

# DESCRIPTION

The **interface** plugin collects information about the traffic,
packets per second and errors of interfaces.
If you're not interested in all interfaces but want to exclude some, or only
collect information of some selected interfaces, you can select the
&#8220;interesting&#8221; interfaces using the plugin's configuration.

The plugin support the following options:

**interface** \[*incl|include|excl|exclude*] *interface*

> If no configuration is given, the **interface** plugin will collect data from
> all interfaces.
> This may not be practical, especially for loopback and similar interfaces.
> Thus, you can use the **interface** option to pick the interfaces you're
> interested in.
> Sometimes, however, it's easier/preferred to collect all interfaces
> *except* a few ones.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**report-inactive** *true|false*

> When set to *false*, only interfaces with non-zero traffic will be
> reported.
> Note that the check is done by looking into whether a package was sent at any
> time from boot and the corresponding counter is non-zero.
> So, if the interface has been sending data in the past since boot, but not
> during the reported time-interval, it will still be reported.
> The default value is *true* and results in collection of the data
> from all interfaces that are selected by **interface**.

**unique-name** *true|false*

> Interface name is not unique on Solaris (KSTAT), interface name is unique
> only within a module/instance.
> Following tuple is considered unique: (ks\_module, ks\_instance, ks\_name).
> If this option is set to true, interface name contains above three fields
> separated by an underscore.

> This option is only available on Solaris.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

