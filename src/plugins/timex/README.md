NCOLLECTD-TIMEX(5) - File Formats Manual

# NAME

**ncollectd-timex** - Documentation of ncollectd's timex plugin

# SYNOPSIS

	load-plugin timex
	plugin timex

# DESCRIPTION

The **timex** plugin exports state of kernel time synchronization flag
that should be maintained by time-keeping daemon and is eventually raised
by Linux kernel if time-keeping daemon does not update it regularly.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

