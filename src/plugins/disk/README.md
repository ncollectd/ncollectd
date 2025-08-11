NCOLLECTD-DISK(5) - File Formats Manual

# NAME

**ncollectd-disk** - Documentation of ncollectd's disk plugin

# SYNOPSIS

	load-plugin disk
	plugin disk {
	    disk [incl|include|excl|exclude] name
	    use-bsd-name true|false
	    udev-name-attr attribute
	}

# DESCRIPTION

The **disk** plugin collects information about the usage of physical
disks and logical disks (partitions).
Values collected are the number of octets written to and read from a disk
or partition, the number of read/write operations issued to the disk and a
rather complex "time" it took for these commands to be issued.

Using the following options you can ignore some disks or configure the
collection only of specific disks.

**disk** \[*incl|include|excl|exclude*] *name*

> Select the disk *name*.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**use-bsd-name** *true|false*

> Whether to use the device's "BSD Name", on MacOSX, instead of the
> default major/minor numbers.
> Requires collectd to be built with Apple's IOKitLib support.

**udev-name-attr** *attribute*

> Attempt to override disk instance name with the value of a specified udev
> attribute when built with **libudev**.
> If the attribute is not defined for the given device, the default name is used.
> Example:

> > UdevNameAttr "DM\_NAME"

> Please note that using an attribute that does not differentiate between the
> whole disk and its particular partitions (like **ID\_SERIAL**) will result in
> data about the whole disk and each partition being mixed together incorrectly.

> The rules below provide a **ID\_NCOLLECTD** attribute instead, which
> differentiates between the whole disk and its particular partitions.

> > ACTION=="remove", GOTO="ncollectd\_end"
> > 
> > SUBSYSTEM!="block", GOTO="ncollectd\_end"
> > 
> > ENV{DEVTYPE}=="disk", ENV{ID\_SERIAL}=="?\*", &#92;
> >     ENV{ID\_NCOLLECTD}="$env{ID\_SERIAL}-disk"
> > ENV{DEVTYPE}=="partition", ENV{ID\_SERIAL}=="?\*", ENV{PARTN}=="?\*", &#92;
> >     ENV{ID\_NCOLLECTD}="$env{ID\_SERIAL}-part$env{PARTN}"
> > 
> > LABEL="ncollectd\_end"

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
