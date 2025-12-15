NCOLLECTD-NOTIFY\_EMAIL(5) - File Formats Manual

# NAME

**ncollectd-notify\_email** - Documentation of ncollectd's notify\_email plugin

# SYNOPSIS

	load-plugin notify_email
	plugin notify_email {
	    recipient address
	    smtp-server hostname
	    smtp-port port
	    smtp-user username
	    smtp-password password
	    from address
	    subject subject
	}

# DESCRIPTION

The **notify\_email** plugin  uses the *ESMTP* library to send
notifications to a configured email address.
*libESMTP* is available from
[http://www.stafford.uklinux.net/libesmtp/](http://www.stafford.uklinux.net/libesmtp/)

Available configuration options:

**recipient** *address*

> Configures the email address(es) to which the notifications should be mailed.
> May be repeated to send notifications to multiple addresses.

> At least one **recipient** must be present for the plugin to work correctly.

**smtp-server** *hostname*

> Hostname of the SMTP server to connect to.
> Default to localhost.

**smtp-port** *port*

> TCP port to connect to.
> Default to 25

**smtp-user** *username*

> Username for ASMTP authentication.
> Optional.

**smtp-password** *password*

> Password for ASMTP authentication.
> Optional.

**from** *address*

> Email address from which the emails should appear to come from.
> Default to root@localhost.

**subject** *subject*

> Subject-template to use when sending emails.
> There must be exactly two string-placeholders in the subject,
> given in the standard
> printf(3)
> syntax, i. e. %s.
> The first will be replaced with the severity, the second with the hostname.
> Default: ncollectd notify: %s@%s

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

