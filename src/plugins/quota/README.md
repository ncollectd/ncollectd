NCOLLECTD-QUOTA(5) - File Formats Manual

# NAME

**ncollectd-quota** - Documentation of ncollectd's quota plugin

# SYNOPSIS

	load-plugin quota
	plugin quota {
	    device [incl|include|excl|exclude] device-name
	    mount-point [incl|include|excl|exclude] mount-point
	    fs-type [incl|include|excl|exclude] fs-type
	    user-id [incl|include|excl|exclude]  uid
	    user-name [incl|include|excl|exclude] username
	    report-by-user-name true|false
	    report-by-device true|false
	    report-zero true|false
	    reload-users-interval seconds
	}

# DESCRIPTION

The **quota** plugin collect metrics of filesystems quota.

The plugin supports the following options:

**device** \[*incl|include|excl|exclude*] *devicename*

> Select filesystems based on the devicename.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**mount-point** \[*incl|include|excl|exclude*] *mount-point*

> Select filesystems based on the mountpoint.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**fs-type** \[*incl|include|excl|exclude*] *fs-type*

> Select filesystems based on the filesystem type.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**user-id** \[*incl|include|excl|exclude*] *uid*

> Select users quotas based on the uid.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**user-name** \[*incl|include|excl|exclude*] *username*

> Select users quotas based on the username.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

**report-by-user-name** *true|false*

> If true, the users are selected with the **user-name** option, otherwise
> with the **user-id** option. Defaults to true.

**report-by-device** *true|false*

> If true, the label *disk* is the device name (without the leading "/dev/"),
> otherwise the mount point. Defaults to false.

**report-zero** *true|false*

> If true, report the quotas of the users even if they do not use any inode
> or disk space. Defaults to false.

**reload-users-interval** *seconds*

> Interval in seconds to reload the list of users. If 0 the list is reloaded
> when /etc/passwd is modified. Defaults to 0.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

