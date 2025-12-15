NCOLLECTD-NUMA(5) - File Formats Manual

# NAME

**ncollectd-numa** - Documentation of ncollectd's numa plugin

# SYNOPSIS

	load-plugin numa
	plugin numa

# DESCRIPTION

The **numa** plugin reports statistics of the Non-Uniform Memory Access
(NUMA) subsystem of Linux.
It uses the information reported in
*/sys/devices/system/node/nodeXY/numastat*.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

