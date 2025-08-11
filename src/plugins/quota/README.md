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

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
