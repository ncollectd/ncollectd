NCOLLECTD-SSSD(5) - File Formats Manual

# NAME

**ncollectd-sssd** - Documentation of ncollectd's sssd plugin

# SYNOPSIS

	load-plugin sssd
	plugin sssd

# DESCRIPTION

The **sssd** plugin collects information from SSSD daemon.

The information is read from the SSSD InfoPipe D-Bus service
(*org.freedesktop.sssd.infopipe*), so the **ifp** responder must be enabled
in sssd.conf(5) and the user running ncollectd must be listed in the
**allowed\_uids** option of the **\[ifp]** section, e.g.:

	[sssd]
	services = nss, pam, ifp

	[ifp]
	allowed_uids = root

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

