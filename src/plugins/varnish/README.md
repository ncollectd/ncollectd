NCOLLECTD-VARNISH(5) - File Formats Manual

# NAME

**ncollectd-varnish** - Documentation of ncollectd's varnish plugin

# SYNOPSIS

	load-plugin varnish
	plugin varnish {
	    instance name {
	        vsh-instance vsh-name
	        collect flags
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **vanish** plugin collects information about Varnish, an HTTP accelerator.

The configuration consists of one or more **instance** blocks.
Inside each **instance** blocks, the following options are recognized:

**vhs-instance** *vsh-instanace-name*

> The *vsh-instanace-name* is the parameter passed to "varnishd -n".
> If left empty, it will collectd statistics from the default "varnishd" instance
> (this should work fine in most cases).

**label** *key* *value*

**interval** *seconds*

**collect** *flags*

> **backend**

> > Back-end connection statistics, such as successful, reused,
> > and closed connections.
> > Collected by default.

> **cache**

> > Cache hits and misses.
> > Collected by default.

> **connections**

> > Number of client connections received, accepted and dropped.
> > Collected by default.

> **dirdns**

> > DNS director lookup cache statistics.
> > Only available with Varnish 3.x.
> > Not collected by default.

> **esi**

> > Edge Side Includes (ESI) parse statistics.
> > Not collected by default.

> **fetch**

> > Statistics about fetches (HTTP requests sent to the backend).
> > Not collected by default.

> **hcb**

> > Inserts and look-ups in the crit bit tree based hash.
> > Look-ups are divided into locked and unlocked look-ups.
> > Not collected by default.

> **objects**

> > Statistics on cached objects: number of objects expired, nuked (prematurely
> > expired), saved, moved, etc.
> > Not collected by default.

> **ban**

> > Statistics about ban operations, such as number of bans added, retired, and
> > number of objects tested against ban operations.
> > Not collected by default.

> **session**

> > Client session statistics.
> > Number of past and current sessions, session herd and linger counters, etc.
> > Not collected by default.

> **shm**

> > Statistics about the shared memory log, a memory region to store
> > log messages which is flushed to disk when full.
> > Collected by default.

> **sma**

> > Malloc or umem (
> > umem\_alloc(3MALLOC)
> > based) storage statistics.
> > The umem storage component is Solaris specific.
> > Note: SMA, SMF and MSE share counters, enable only the one used by
> > the Varnish instance.
> > Not collected by default.

> **sms**

> > Synth (synthetic content) storage statistics.
> > This storage component is used internally only.
> > Not collected by default.

> **struct**

> > Current varnish internal state statistics.
> > Number of current sessions, objects in cache store etc.
> > Not collected by default.

> **totals**

> > Collects overview counters, such as the number of sessions created,
> > the number of requests and bytes transferred.
> > Not collected by default.

> **uptime**

> > Varnish uptime.
> > Not collected by default.

> **vcl**

> > Number of total (available + discarded) VCL (config files).
> > Not collected by default.

> **workers**

> > Collect statistics about worker threads.
> > Not collected by default.

> **vsm**

> > Collect statistics about Varnish's shared memory usage (used by the logging and
> > statistics subsystems).
> > Only available with Varnish 4.x.
> > Not collected by default.

> **lck**

> > Lock counters.
> > Not collected by default.

> **mempool**

> > Memory pool counters.
> > Not collected by default.

> **mgt**

> > Management process counters.
> > Not collected by default.

> **smf**

> > File (memory mapped file) storage statistics.
> > Note: SMA, SMF and MSE share counters, enable only the one used by
> > the Varnish instance.

> **vbe**

> > Backend counters.
> > Only available with Varnish 4.x and above.
> > Not collected by default.

> **mse**

> > Varnish Massive Storage Engine 2.0 (MSE2) is an improved storage backend for
> > Varnish, replacing the traditional malloc and file storages.
> > Only available with Varnish-Plus 4.x and above.
> > Note: SMA, SMF and MSE share counters, enable only the one used by
> > the Varnish instance.
> > Not collected by default.

> **goto**

> > vmod-goto counters.
> > Only available with Varnish Plus 6.x.
> > Not collected by default.

> **smu**

> **brotli**

> **accg\_diag**

> **accg**

> **workspace**

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
