NCOLLECTD-CPUSLEEP(5) - File Formats Manual

# NAME

**ncollectd-cpusleep** - Documentation of ncollectd's cpusleep plugin

# SYNOPSIS

	load-plugin cpusleep
	plugin cpusleep

# DESCRIPTION

The **cpusleep** plugin doesn't have any options.
It reads CLOCK\_BOOTTIME and CLOCK\_MONOTONIC and reports the
difference between these clocks.
Since **BOOTTIME** clock increments while device is suspended and
**MONOTONIC** clock does not, the derivative of the difference between
these clocks gives the relative amount of time the device has spent in
suspend state.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

