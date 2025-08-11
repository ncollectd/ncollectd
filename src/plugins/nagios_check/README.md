NCOLLECTD-NAGIOS\_CHECK(5) - File Formats Manual

# NAME

**ncollectd-nagios\_check** - Documentation of ncollectd's nagios\_check plugin

# SYNOPSIS

	plugin nagios_check {
	    instance name {
	        cmd program [args] ...
	        user username
	        group groupname
	        env key value
	        interval seconds
	        refresh seconds
	        persist true|false
	        persist-ok true|false
	        notification name
	        label key value
	        annotation key value
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **nagios\_check** plugin forks off a Nagios Plugin and dispatch the
notifications with the state change.

The **instance** defines the Nagios Plugin an program to exec, and has
the following options:

**cmd** *program* \[*args*] ...

> The program to execute it may be followed by optional arguments that are
> passed to the program.
> Please note that due to the configuration parsing numbers and boolean
> values may be changed.
> If you want to be absolutely sure that something is passed as-is please
> enclose it in quotes.

**user** *username*

> Execute the executable **cmd** as user *user*.

**group** *groupname*

> If the **group** is set, the effective group of the executed program
> is set to that group.
> Please note that in order to change the user and/or group the daemon needs
> superuser privileges.
> If the daemon is run as an unprivileged user you must specify the same
> user/group here.
> If the daemon is run with superuser privileges, you must supply a non-root
> user here.

**env** *key* *value*

> Set the eviroment variable key=value for the executed program.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**refresh** *seconds*

> Sets the refresh interval (in seconds) to fire the notification again
> even if the state has not changed.

**persist** *true|false*

> Fire the notification again even if the state has not changed.

**persist-ok** *true|false*

> Fire the notification again when the state is OKAY and the state
> has not changed.

**notification** *name*

> Sets de notification name.

**label** *key* *value*

> Append the label *key*=*value* to the notification.
> Can appear multiple time in the **instance** block.

**annotation** *key* *value*

> Append the annotation *key*=*value* to the notification.
> Can appear multiple time in the **instance** block.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
