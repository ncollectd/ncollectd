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
	    starttls disable|enable|require
	    timeout seconds
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

**starttls** *disable|enable|require*

> Use STARTTLS to encrypt the connection to the SMTP server.
> With **enable** TLS is used if offered by the server, with **require**
> the session fails if the server does not offer it.
> Default to **disable**.

**timeout** *seconds*

> Timeout for each phase of the SMTP session.
> Default to the libESMTP timeouts (several minutes, as per RFC 5321).

**from** *address*

> Email address from which the emails should appear to come from.
> Default to root@localhost.

**subject** *subject*

> Prefix of the subject used when sending emails.
> The notification name and severity are appended to it.
> Default: "NCollectd notify: "

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

