NCOLLECTD-PODMAN(5) - File Formats Manual

# NAME

**ncollectd-podman** - Documentation of ncollectd's podman plugin

# SYNOPSIS

	load-plugin podman
	plugin podman {
	    instance instance {
	        url url
	        labels key value
	        timeout seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **podman** plugin collect metrics from podman managed containers.

In the **plugin** block, there may be one or more **instance** blocks,
each defining a podman daemon.
The following options are valid within **instance** blocks:

**url** *url*

> Podman instance *url* to connect.

**labels** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for request to the podman
> instance.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
