NCOLLECTD-PROCESSES(5) - File Formats Manual

# NAME

**ncollectd-processes** - Documentation of ncollectd's processes plugin

# SYNOPSIS

	load-plugin processes
	plugin processes {
	    collect flags ...
	    process name {
	        collect flags ...
	        filter {
	            ...
	        }
	    }
	    process-match name /regex/ {
	        collect flags ...
	        filter {
	            ...
	        }
	    }
	    process-pidfile name pidfile-path {
	        collect flags ...
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **processes** plugin collects information about processes of local system.

By default, with no process matches configured, only general statistics is
collected: the number of processes in each state and fork rate.

Process matches can be configured by **process** and **process-match**
options.
These may also be a block in which further options may be specified.

The statistics collected for matched processes are:

*	size of the resident segment size (RSS)

*	user-time and system-time used

*	number of processes

*	number of threads

*	number of open files (under Linux)

*	number of memory mapped files (under Linux)

*	io data (under Linux),
	requires the capabilities: CAP\_DAC\_OVERRIDE and CAP\_SYS\_PTRACE

*	context switches (under Linux)

*	minor and major pagefaults

*	delay Accounting information (Linux only, requires libmnl)

The **processes** plugin has the following options:

**process** *name*

> Select more detailed statistics of processes matching this name.
> Some platforms have a limit on the length of process names.
> *name* must stay below this limit.
> In Linux the *name* it's chequed against the second field of
> /proc/pid/stat, the filename of the executable, in parentheses.

**process-match** *name* *regex*

> Select more detailed statistics of processes matching the specified *regex*
> (see.
> regex(7)
> for details).
> The statistics of all matching processes are summed up and dispatched to the
> daemon using the specified *name* as an identifier.
> This allows one to "group" several processes together.

**process-pidfile** *name* *pidfile-path*

>  Select more detailed statistics of processes matching and specified *PID*
> contained in the file *pidfile-path*.

**collect** *flags* ...

> Must be defined before **process** and **process-match**.
> See **COLLECT FLAGS** from
> ncollectd.conf(5).

> **delay\_accounting**

> > If enabled, collect Linux Delay Accounting information for matching processes.
> > Delay Accounting provides the time processes wait for the CPU to become
> > available, for I/O operations to finish, for pages to be swapped in and for
> > freed pages to be reclaimed.
> > Disabled by default.

> > This option is only available on Linux, requires the **libmnl** library and
> > requires the CAP\_NET\_ADMIN capability at runtime.

> **file\_descriptor**

> > Collect number of file descriptors of matched processes.
> > Disabled by default.

> **memory\_maps**

> > Collect the number of memory mapped files of the process.
> > The limit for this number is configured via */proc/sys/vm/max\_map\_count* in
> > the Linux kernel.

> The **collect** options may be used inside
> **process** and **process-match** blocks.
> When used there, these options affect reporting the corresponding processes
> only.
> Outside of **process**, **process-match** and **process-pidfile** blocks
> these options set the default value for subsequent matches.

Inside **process**, **process-match** and **process-pidfile** blocks
we can define a filter:

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

