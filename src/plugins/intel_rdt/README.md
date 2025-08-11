NCOLLECTD-APACHE(5) - File Formats Manual

# NAME

**ncollectd-apache** - Documentation of ncollectd's apache plugin

# SYNOPSIS

	load-plugin intel_rdt
	plugin intel_rdt {
	    cores cores ...
	    processes "process" ...
	    mon-ipc-enabled true|false
	    mon-llc-ref-enabled true|false
	}

# DESCRIPTION

The **intel\_rdt** plugin collects information provided by monitoring features
of Intel Resource Director Technology (Intel(R) RDT) like Cache Monitoring
Technology (CMT), Memory Bandwidth Monitoring (MBM). These features provide
information about utilization of shared resources.
CMT monitors last level cache occupancy (LLC).
MBM supports two types of events reporting local and remote memory bandwidth.
Local memory bandwidth (MBL) reports the bandwidth of accessing memory
associated with the local socket.
Remote memory bandwidth (MBR) reports the bandwidth of accessing the remote
socket.
Also this technology allows to monitor instructions per clock (IPC).
Monitor events are hardware dependant.
Monitoring capabilities are detected on plugin initialization and only
supported events are monitored.

**intel\_rdt** plugin is using model-specific registers (MSRs), which
require an additional capability (**CAP\_SYS\_RAWIO**) to be enabled if
ncollectd is run as a service.

By default global interval is used to retrieve statistics on monitored
events.
To configure a plugin specific interval use **interval** option of the
intel\_rdt **load-plugin** block.
For milliseconds divide the time by 1000 for example if the desired interval
is 50ms, set interval to 0.05.
Due to limited capacity of counters it is not recommended to set interval higher
than 1 sec.

The following configuration options are available:

**cores** *cores* ...

> Monitoring of the events can be configured for group of cores
> (aggregated statistics).
> This field defines groups of cores on which to monitor supported events.
> The field is represented as list of strings with core group values.
> Each string represents a list of cores in a group.
> Allowed formats are:

> >     0,1,2,3
> >     0-10,20-18
> >     1,3,5-8,10,0x10-12

> If an empty string is provided as value for this field default cores
> configuration is applied - a separate group is created for each core.

**processes** *process* ...

> Monitoring of the events can be configured for group of processes
> (aggregated statistics).
> This field defines groups of processes on which to monitor supported events.
> The field is represented as list of strings with process names group values.
> Each string represents a list of processes in a group.
> Allowed format is:

> >     sshd,bash,qemu

**mon-ipc-enabled** *true|false*

> Determines whether or not to enable IPC monitoring.
> If set to *true* (the default), IPC monitoring statistics will be
> collected by intel\_rdt plugin.

**mon-llc-ref-enabled** *true|false*

> Determines whether or not to enable LLC references monitoring.
> If set to *false* (the default), LLC references monitoring statistics
> will not be collected by intel\_rdt plugin.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
