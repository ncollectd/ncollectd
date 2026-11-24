NCOLLECTD-LVM(5) - File Formats Manual

# NAME

**ncollectd-lvm** - Documentation of ncollectd's lvm plugin

# SYNOPSIS

	load-plugin lvm
	plugin lvm

# DESCRIPTION

The **lvm** plugin collects metrics form the Logical Volume Manager (LVM),
of the usage of Volume Grousp (VG) and Logical Volumes (LV).

The plugin runs **/usr/sbin/lvm fullreport** with **--reportformat json_std**,
which requires lvm2 version 2.03.17 or later.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

