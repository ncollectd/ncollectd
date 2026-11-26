NCOLLECTD-NOTIFY\_EXEC(5) - File Formats Manual

# NAME

**ncollectd-notify\_exec** - Documentation of ncollectd's notify\_exec plugin

# SYNOPSIS

	load-plugin notify_exec
	plugin notify_exec {
	    format text|json|protob|env|environment
	    if-match match {
	        cmd program [args] ...
	        user username
	        group groupname
	        env key value
	        format text|json|protob|env|environment
	        timeout seconds
	    }
	}

# DESCRIPTION

The **notify\_exec** plugin forks off an executable to dispatch
notifications to the outside world.

**if-match** *match*

> The program is forked once for each notification that is handled by the daemon.
> The notification is passed to the program on STDIN in a fashion similar
> to HTTP-headers.
> Only one instance of the program is run at a time for each **if-match**
> block: notifications received while the program is still running are
> discarded.

> **cmd** *program* \[*args*] ...

> **user** *username*

> > Execute the executable **cmd** as user *user*.

> **group** *groupname*

> > If the **group** is set, the effective group of the executed program
> > is set to that group.
> > Please note that in order to change the user and/or group the daemon needs
> > superuser privileges.
> > If the daemon is run as an unprivileged user you must specify the same
> > user/group here.
> > If the daemon is run with superuser privileges, you must supply a non-root
> > user here.

> **env** *key* *value*

> > Set the eviroment variable key=value for the executed program.
> > The program does not inherit the environment of the daemon, only the
> > variables set with **env** (and the notification variables with the
> > *env* format) are passed, so set **PATH** if the program needs it.

> **format** *text|json|protob|env|environment*

> > How the notification is passed to the program.

> **timeout** *seconds*

> > Maximum time to wait for the program to read the notification and exit.
> > When the timeout expires the program is terminated with **SIGTERM**
> > and, if it does not exit in one second, with **SIGKILL**.
> > Set to 0 to wait forever.
> > Defaults to 60 seconds.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

