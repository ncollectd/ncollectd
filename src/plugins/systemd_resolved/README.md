NCOLLECTD-SYSTEMD\_RESOLVED(5) - File Formats Manual

# NAME

**ncollectd-systemd\_resolved** - Documentation of ncollectd's systemd\_resolved plugin

# SYNOPSIS

	load-plugin systemd_resolved
	plugin systemd_resolved {
	    socket-path path
	}

# DESCRIPTION

The **systemd\_resolved** plugin collects statistics from
**systemd-resolved**.
It calls the *io.systemd.Resolve.Monitor.DumpStatistics* varlink method,
available since systemd 252.
The monitor socket is only accessible by root.

**socket-path** *path*

> Path of the systemd-resolved monitor varlink socket.
> Defaults to */run/systemd/resolve/io.systemd.Resolve.Monitor*.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

