NCOLLECTD-EXEC(5) - File Formats Manual

# NAME

**ncollectd-exec** - Documentation of ncollectd's exec plugin

# SYNOPSIS

	load-plugin exec
	plugin exec {
	    instance name {
	        cmd program [args] ...
	        user username
	        group groupname
	        env key value
	        interval seconds
	        label key value
	        metric-prefix prefix
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **exec** plugin forks off an executable either to receive values or to
dispatch notifications to the outside world.

The **instance** defines an program to exec.
These programs are forked and values that it writes to STDOUT are
read back.
The executable is forked in a fashion similar to init: It is forked
once and not again until it exits.
If it exited, it will be forked again after at most *interval* seconds.
It is perfectly legal for the executable to run for a long time and
continuously write values to STDOUT.

The environment variable NCOLLECTD\_INTERVAL is set by the plugin before
calling *exec*, with the **interval** configured for this **exec** or
the global **interval** value if not set.

If the executable only writes one value and then exits it will be
executed every **interval** seconds.
If **interval** is short (the default is 10 seconds) this may result in
serious system load.

The forked executable is expected to print metrics to STDOUT.
The expected format is as follows:

When ncollectd exits it sends a SIGTERM to all still running
child-processes upon which they have to quit.

**cmd** *program* \[*args*] ...

> The program to execute it may be followed by optional arguments that are passed
> to the program.
> Please note that due to the configuration parsing numbers and boolean values
> may be changed.
> If you want to be absolutely sure that something is passed as-is please
> enclose it in quotes.

**user** *username*

> Execute the executable **cmd** as user *user*.

**group** *groupname*

> Execute the executable **cmd** as user *user*.
> If the **group** is set, the effective group of the executed program is set
> to that group.
> Please note that in order to change the user and/or group the daemon needs
> superuser privileges.
> If the daemon is run as an unprivileged user you must specify the same
> user/group here.
> If the daemon is run with superuser privileges, you must supply a
> non-root user here.

**env** *key* *value*

> Set the eviroment variable key=value for the executed program.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**metric-prefix** *prefix*

> Prepend the *prefix* to the metrics read from the exec program.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

The user, the binary is executed as, may not have root privileges, i.e.
must have an UID that is non-zero.
This is for your own good.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
