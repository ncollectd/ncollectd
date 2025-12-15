NCOLLECTD-CONTEXTSWITCH(5) - File Formats Manual

# NAME

**ncollectd-contextswitch** - Documentation of ncollectd's contextswitch plugin

# SYNOPSIS

	load-plugin contextswitch
	plugin contextswitch

# DESCRIPTION

The **contextswitch** plugin collects the number of context switches
that occur in the system.

A context switch is described as the kernel suspending execution of one
process on the CPU and resuming execution of some other process that had
previously been suspended.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

