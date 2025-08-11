NCOLLECTD-APCUPS(5) - File Formats Manual

# NAME

**ncollectd-apcups** - Documentation of ncollectd's apcups plugin

# SYNOPSIS

	load-plugin apcups
	plugin apcups {
	    instance instance {
	        host hostname
	        port port
	        persistent-connection true|false
	        label "key" "value"
	        interval seconds
	    }
	}

# DESCRIPTION

The **apcups** plugin connects to an instance of **apcupsd** to read
various statistics about a connected uninterruptible power supply (UPS),
such as voltage, load, etc.

The configuration of the **apcups** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
The following options are accepted within each **instance** block:

**host** *hostname*

> Hostname of the host running **apcupsd**.
> Defaults to *localhost*.
> Please note that IPv6 support has been disabled unless someone can confirm
> or decline that **apcupsd** can handle it.

**port** *port*

> TCP port to connect to.
> Defaults to **3551**.

**persistent-connection** *true|false*

> The plugin is designed to keep the connection to **apcupsd** open between
> reads.
> If plugin poll interval is greater than 15 seconds (hardcoded socket close
> timeout in **apcupsd** NIS), then this option is *false* by default.

> You can instruct the plugin to close the connection after each read by setting
> this option to *false* or force keeping the connection by setting it to
> **true**.

> If **apcupsd** appears to close the connection due to inactivity quite
> quickly, the plugin will try to detect this problem and switch to an
> open-read-close mode.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected.
> By default the global **interval** setting will be used.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
