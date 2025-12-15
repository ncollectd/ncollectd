NCOLLECTD-SYSTEMD(5) - File Formats Manual

# NAME

**ncollectd-systemd** - Documentation of ncollectd's systemd plugin

# SYNOPSIS

	load-plugin systemd
	plugin systemd {
	    unit unit-name ...
	    slice slice-name ...
	    service service-name ...
	    socket socket-name ...
	    timer timer-name ...
	}

# DESCRIPTION

The **systemd** plugin collects information from systemd supervised unit.
If no units are specified then all units are collected.

The following configuration options are available:

**unit** *unit-name* ...

> Collect information of the specified units names.
> The unit name consist of a unit name prefix and a suffix specifying the unit
> type which begins with a dot.

**slice** *slice-name* ...

> Collect information of the specified slices names.
> It is not necessary to add the ".slice" suffix.

**service** *service-name* ...

> Collect information of the specified services names.
> It is not necessary to add the ".service" suffix.

**socket** *socket-name* ...

> Collect information of the specified sockets names.
> It is not necessary to add the ".socket" suffix.

**timer** *timer-name* ...

> Collect information of the specified timers names.
> It is not necessary to add the ".timer" suffix.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

