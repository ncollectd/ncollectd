NCOLLECTD-SMART(5) - File Formats Manual

# NAME

**ncollectd-smart** - Documentation of ncollectd's smart plugin

# SYNOPSIS

	load-plugin smart
	plugin smart {
	    disk [incl|include|excl|exclude] disk-name
	    serial [incl|include|excl|exclude] serial-number
	    ignore-sleep-mode true|false
	}

# DESCRIPTION

The **smart** plugin collects SMART information from physical
disks.
Values collectd include temperature, power cycle count, poweron
time and bad sectors.
Also, all SMART attributes are collected along with the normalized current
value, the worst value, the threshold and a human readable value.
The plugin can also collect SMART attributes for NVMe disks
(present in accordance with NVMe 1.4 spec) and additional
SMART Attributes from Intel&#174; NVMe disks.

Using the following two options you can ignore some disks or configure the
collection only of specific disks.

**disk** \[*incl|include|excl|exclude*] **disk-name**

> Select the disk *disk-name* to collect metrics.
> If no **disk** option is configured, all disks are collected.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**serial** \[*incl|include|excl|exclude*] **serial-number**

> Select the disk serial number *serial-name* to collect metrics.
> If no **serial** option is configured, all disks are collected.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**ignore-sleep-mode** *true|false*

> Normally, the **smart** plugin will ignore disks that are reported
> to be asleep.
> This option disables the sleep mode check and allows the plugin to collect
> data from these disks anyway.
> This is useful in cases where libatasmart mistakenly reports disks as asleep
> because it has not been updated to incorporate support
> for newer idle states in the ATA spec.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

