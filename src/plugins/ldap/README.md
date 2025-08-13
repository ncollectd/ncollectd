NCOLLECTD-LDAP(5) - File Formats Manual

# NAME

**ncollectd-ldap** - Documentation of ncollectd's ldap plugin

# SYNOPSIS

	load-plugin ldap
	plugin ldap {
	    query name {
	        base searchbase
	        scope base|one|sub|children
	        filter filter
	        attrs attribute [attribute ...]
	        label key value
	        label-from attribute
	        metric-prefix prefix
	        metric {
	            dn DN
	            cn CN
	            metric-prefix prefix
	            metric name
	            metric-from attribute
	            help help
	            help-from attribute
	            type counter|gauge
	            label key value
	            label-from key attribute
	            value-from attribute
	        }
	    }
	    instance name {
	        url ldap://host/binddn
	        bind-dn DN
	        password password
	        password-env env-name
	        ca-cert /path/to/ca
	        start-tls true|false
	        timeout seconds
	        verify-host true|false
	        version version
	        interval seconds
	        metric-prefix prefix
	        label key value
	        query name [name ...]
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The ldap plugin connect to a ldap server to do search queries and
read back the results.
You can configure how each result is interpreted and the plugin
will generate one or more metrics por each result.

The configuration of the **ldap** plugin consists of one or more
**instance** and **query** blocks.
Each block requires one string argument as the name.

**instance** *database*

> The instance blocks define a connection to a ldap server and which search
> queries should be sent to that server.
> The instance name will be used as the label instance.
> In order for the plugin to work correctly, each instance name must be unique.
> This is not enforced by the plugin and it is your responsibility to
> ensure it is.

> **url** *ldap://host/binddn*

> > Sets the URL to use to connect to the OpenLDAP server.
> > This option is mandatory.

> **bind-dn** *DN*

> > Name in the form of an LDAP distinguished name intended to be used for
> > authentication.
> > Defaults to empty string to establish an anonymous authorization.

> **password** *password*

> > Password for simple bind authentication.
> > If this option is not set, unauthenticated bind operation is used.

> **password-env** *env-name*

> > Get the password for simple bind authentication from the environment
> > variable *env-name*.

> **ca-cert** */path/to/ca*

> > File that holds one or more SSL certificates.
> > If you want to use TLS/SSL you may possibly need this option.
> > What CA certificates are checked by default depends on the distribution
> > you use and can be changed with the usual ldap client configuration mechanisms.
> > See
> > ldap.conf(5)
> > for the details.

> **start-tls** *true|false*

> > Defines whether TLS must be used when connecting to the OpenLDAP server.
> > Disabled by default.

> **timeout** *seconds*

> > Sets the timeout value for ldap operations, in seconds.
> > By default, the configured *interval* is used to set the timeout.
> > Use **-1** to disable (infinite timeout).

> **verify-host** *true|false*

> > Enables or disables peer host name verification.
> > If enabled, the plugin checks if the Common Name or a
> > Subject Alternate Name field of the SSL certificate matches the
> > host name provided by the **url** option.
> > If this identity check fails, the connection is aborted.
> > Enabled by default.

> **version** *version*

> > An integer which sets the LDAP protocol version number to use when connecting
> > to the OpenLDAP server.
> > Defaults to **3** for using *LDAPv3*.

> **interval** *seconds*

> > Sets the interval (in seconds) in which the values will be collected from this
> > URL.
> > By default the global **interval** setting will be used.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metric name.

> **query** *name* \[*name* ...]

> > Associates the query named *name* with this database connection.
> > The query needs to be defined *before* this statement, i. e. all query
> > blocks you want to refer to must be placed above the instance block you want to
> > refer to them from.

> **filter**

> > Configure a filter to modify or drop the metrics.
> > See **FILTER CONFIGURATION** in
> > ncollectd.conf(5).

**query** *query-name*

> The query blocks define a ldap search and how the returned data should
> be interpreted.

> **base** *searchbase*

> > Use *searchbase* as the starting point for the search instead
> > of the default.

> **scope** *base|one|sub|children*

> >  Specify  the scope of the search to be one of *base*, *one*,
> > *sub* *children* to specify a base object, one-level, subtree,
> > or children search.
> > The default is *sub*.

> **filter** *filter*

> > Set the filter for the search.
> > The filter should conform to the string representation for search filters
> > as defined in RFC 4515.
> > If not provided, the default filter, (objectClass=\*), is used.

> **attrs** *attribute* \[*attribute* ...]

> > Set the list of attributes that will be returned.
> > If no attributes are set, all user attributes are returned.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **label-from** *attribute*

> > Specifies the attribute whose values will be used to create the labels.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metric name.

> **metric**

> > **dn** *DN*

> > > Only apply this metric definition to entries that match this *DN*.

> > **cn** *CN*

> > > Only apply this metric definition to entries that match this *CN*.

> > **metric-prefix** *prefix*

> > > Prepends *prefix* to the metric name.

> > **metric** *name*

> > > Set the metric name.

> > **metric-from** *attribute*

> > > Read the metric name from the named attribute.
> > > There must be at least one **metric** or **metric-from** option inside
> > > each **metric** block.

> > **help** *help*

> > > Set the **help** text for the metric.

> > **help-from** *attribute*

> > > Read the **help** text for the the metric from the named attribute.

> > **type** *counter|gauge*

> > > The **type** that's used for each metric returned.
> > > Must be *gauge* or *counter*.

> > **label** *key* *value*

> > > Append the label *key*=*value* to the submitting metrics.
> > > Can appear multiple times in the **metric** block.

> > **label-from** *key* *attribute*

> > > Specifies the attribute whose values will be used to create the labels.

> > **value-from** *attribute*

> > > Name of the attribute whose content is used as the actual data for the metric
> > > that are dispatched to the daemon.
> > > There must be only one **value-from** option inside each **metric** block.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
