NCOLLECTD-APACHE(5) - File Formats Manual

# NAME

**ncollectd-apache** - Documentation of ncollectd's apache plugin

# SYNOPSIS

	load-plugin notify_exec
	plugin notify_exec {
	    format text|json|protob|env|environment
	    if-match match {
	        cmd program [args] ...
	        user username
	        group Ugroupname
	        env key value
	        format text|json|protob|env|environment
	    }
	}

# DESCRIPTION

The **notify\_exec** plugin forks off an executable to dispatch
notifications to the outside world.

**if-match** *match*

> The program is forked once for each notification that is handled by the daemon.
> The notification is passed to the program on STDIN in a fashion similar
> to HTTP-headers.
> This program is not serialized, so that several instances of this program may
> run at once if multiple notifications are received.

> **cmd** *program* \[*args*] ...

> **user** *username*

> > Execute the executable **cmd** as user *user*.

> **group** *Ugroupname*

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

> **format** *text|json|protob|env|environment*

> > How the notification is passed to the program.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

