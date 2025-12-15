NCOLLECTD-DS389(5) - File Formats Manual

# NAME

**ncollectd-ds389** - Documentation of ncollectd's ds389 plugin

# SYNOPSIS

	load-plugin ds389
	plugin ds389 {
	    instance name {
	        url ldap://host/binddn
	        bind-dn dn
	        password password
	        password-env env-name
	        ca-cert /path/to/ca-cert
	        start-tls true|false
	        timeout seconds
	        verify-host true|false
	        version version
	        label key value
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The ds389 plugin collect metrics from the 389 Directory Server.

The configuration of the **ds389** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.

In order for the plugin to work correctly, each instance name must be unique.
This is not enforced by the plugin and it is your responsibility
to ensure it is.

The following options are accepted within each **instance** block:

**url** *ldap://host/binddn*

> Sets the URL to use to connect to the *ds389* server.
> This option is mandatory.

**bind-dn** *dn*

> Name in the form of an LDAP distinguished name intended to be used for
> authentication.
> Defaults to empty string to establish an anonymous authorization.

**password** *password*

> Password for simple bind authentication.
> If this option is not set, unauthenticated bind operation is used.

**password-env** *env-name*

> Get the password for simple bind authentication from the environment
> variable *env-name*.

**ca-cert** */path/to/ca-cert*

> File that holds one or more SSL certificates.
> If you want to use TLS/SSL you may possibly need this option.
> What CA certificates are checked by default depends on the distribution
> you use and can be changed with the usual ldap client configuration mechanisms.
> See
> ldap.conf(5)
> for the details.

**start-tls** *true|false*

> Defines whether TLS must be used when connecting to the *ds389* server.
> Disabled by default.

**timeout** *seconds*

> Sets the timeout value for ldap operations, in seconds.
> By default, the configured **interval** is used to set the timeout.
> Use **-1** to disable (infinite timeout).

**verify-host** *true|false*

> Enables or disables peer host name verification.
> If enabled, the plugin checks if the Common Name or a
> Subject Alternate Name field of the SSL certificate matches the host
> name provided by the **url** option.
> If this identity check fails, the connection is aborted.
> Enabled by default.

**version** *version*

> An integer which sets the LDAP protocol version number to use when connecting
> to the LDAP server.
> Defaults to **3** for using *LDAPv3*.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> URL.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

