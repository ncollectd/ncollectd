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
	        timeout seconds
	        refresh seconds
	        persist true|false
	        persist-ok true|false
	        notification name
	        label key value
	        annotation key value
	    }
	}

# DESCRIPTION

The **nagios\_check** plugin forks off a Nagios Plugin and dispatch the
notifications with the state change.

The exit code of the Nagios Plugin is mapped to the notification severity:
*0* (OK) to **OKAY**, *1* (WARNING) to **WARNING**, *2* (CRITICAL) to
**FAILURE**, and *3* (UNKNOWN) or any other exit code to **FAILURE**.
UNKNOWN and CRITICAL are tracked as different states.
If the plugin is killed by a signal a **FAILURE** notification is sent.

The **instance** defines the Nagios Plugin an program to exec, and has
the following options:

**cmd** *program* \[*args*] ...

> The program to execute it may be followed by optional arguments that are
> passed to the program.
> This option is mandatory.
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

**timeout** *seconds*

> Maximum time (in seconds) the check is allowed to run.
> If the check does not finish in time it is killed and a **FAILURE**
> notification is sent.
> By default the **interval** of the instance is used.

**refresh** *seconds*

> Sets the refresh interval (in seconds) to fire the notification again
> even if the state has not changed.
> If the state is OKAY the notification is only fired again if
> **persist-ok** is enabled.

**persist** *true|false*

> Fire the notification again every interval even if the state has not changed.
> This option is ignored if **refresh** is set.

**persist-ok** *true|false*

> Fire the notification again when the state is OKAY and the state
> has not changed (every **refresh** interval if set, otherwise every
> interval if **persist** is enabled).

**notification** *name*

> Sets de notification name.
> By default the instance name is used.

**label** *key* *value*

> Append the label *key*=*value* to the notification.
> Can appear multiple time in the **instance** block.

**annotation** *key* *value*

> Append the annotation *key*=*value* to the notification.
> Can appear multiple time in the **instance** block.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

