NCOLLECTD-MMC(5) - File Formats Manual

# NAME

**ncollectd-mmc** - Documentation of ncollectd's mmc plugin

# SYNOPSIS

	load-plugin mmc
	plugin mmc {
	    device [incl|include|excl|exclude] device-name
	}

# DESCRIPTION

The **mmc** plugin reads the percentage of bad blocks on mmc device.
The additional values for block erases and power cycles are also read.

**device** \[*incl|include|excl|exclude*] *device-name*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
