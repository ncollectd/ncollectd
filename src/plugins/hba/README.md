NCOLLECTD-HBA(5) - File Formats Manual

# NAME

**ncollectd-hba** - Documentation of ncollectd's hba plugin

# SYNOPSIS

	load-plugin hba
	plugin hba {
	    hba [incl|include|excl|exclude] device
	    refresh-interval seconds
	}

# DESCRIPTION

The **hba** plugin collect  information about the usage of host bus adapters.
The plugin only collect data from Fibre Channel adapters.

The following configuration options are available:

**hba** \[*incl|include|excl|exclude*] *device*

> Select the hba *device*.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**refresh-interval** *seconds*

> Interval to refresh the hba list from the ODM.
> The default value is 300 seconds.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
