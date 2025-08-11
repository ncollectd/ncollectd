NCOLLECTD-TC(5) - File Formats Manual

# NAME

**ncollectd-tc** - Documentation of ncollectd's tc plugin

# SYNOPSIS

	load-plugin tc
	plugin tc {
	    interface Interface
	    qdisc interface [qdisc]
	    class interface [class]
	    filter interface [filtef]
	    ignore-selected true|false
	}

# DESCRIPTION

The **tc** plugin collect metrics of Traffic Control in the Linux kernel.

**interface** *interface*

> Instruct the plugin to collect Traffic Control statistics for this interface.

> If **interface** is *all*, all interfaces will be selected.

> It is possible to use regular expressions to match interface names, if the
> name is surrounded by */.../* and collectd was compiled with support for
> regexps.
> This is useful if there's a need to collect (or ignore) data for a group of
> interfaces that are similarly named, without the need to explicitly list all
> of them (especially useful if the list is dynamic).
> Examples:

> >     interface "/^eth/"
> >     interface "/^ens\[1-4]$|^enp\[0-3]$/"

> This will match all interfaces with names starting with *eth*,
> all interfaces in range *ens1 - ens4* and *enp0 - enp3*, and for
> verbose metrics all interfaces with names starting with *eno* followed
> by at least one digit.

**qdisc** *interface* \[*qdisc*]

**class** *interface* \[*class*]

**filter** *interface* \[*filter*]

> Collect the octets and packets that pass a certain qdisc, class or filter.

> QDiscs and classes are identified by their type and handle (or classid).
> Filters don't necessarily have a handle, therefore the parent's handle is used.
> The notation used in ncollectd differs from that used in
> tc(1)
> in that it doesn't skip the major or minor number if it's zero and doesn't
> print special ids by their name.
> So, for example, a qdisc may be identified by **pfifo\_fast-1:0** even
> though the minor number of **all** qdiscs is zero and thus not displayed by
> tc(1).

> If **qdisc**, **class**, or **filter** is given without the second
> argument, i.e. without an identifier, all qdiscs, classes, or filters
> that are associated with that interface will be collected.

> Since a filter itself doesn't necessarily have a handle, the parent's handle
> is used.
> This may lead to problems when more than one filter is attached to a
> qdisc or class.
> This isn't nice, but we don't know how this could be done any better.
> If you have a idea, please don't hesitate to tell us.

> As with the **interface** option you can specify **all** as the interface,
> meaning all interfaces.

> Here are some examples to help you understand the above text more easily:

> >     plugin tc {
> >         qdisc "eth0" "pfifo\_fast-1:0"
> >         qdisc "ppp0"
> >         class "ppp0" "htb-1:10"
> >         filter "ppp0" "u32-1:0"
> >     }

**ignore-selected** *true|false*

> If nothing is selected at all, everything is collected.
> If some things are selected using the options described above,
> only these statistics are collected.
> If you set **ignore-selected** to **true**, this behavior is inverted,
> the specified statistics will not be collected.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
