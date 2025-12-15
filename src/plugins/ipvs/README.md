NCOLLECTD-IPVS(5) - File Formats Manual

# NAME

**ncollectd-ipvs** - Documentation of ncollectd's ipvs plugin

# SYNOPSIS

	load-plugin ipvs
	plugin ipvs

# DESCRIPTION

The **ipvs** plugin extracts statistics from IP Virtual Server (IPVS),
the transport-layer load-balancer of the Linux Virtual Server (LVS) project.
It stores traffic and connections history for each of the Real Servers (RS)
behind a local Virtual Server (VS).
Ncollectd must be run on Directors (in LVS jargon).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

