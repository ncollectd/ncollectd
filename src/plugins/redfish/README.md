NCOLLECTD-REDFISH(5) - File Formats Manual

# NAME

**ncollectd-redfish** - Documentation of ncollectd's redfish plugin

# SYNOPSIS

	load-plugin redfish
	plugin redfish {
	    query query {
	        end-point url
	        metric-prefix prefix
	        label key value
	        resource resource {
	            property property {
	                type counter|gauge
	                help help
	                metric metric
	                label key value
	                label-from key attribute
	                scale value
	                shift value
	                select-id id ...
	                select-attr attribute ...
	                select-attr-value key value
	            }
	        }
	        attribute attribute {
	            type counter|gauge
	            help help
	            metric metric
	            label key value
	            label-from key attribute
	            scale value
	            shift value
	        }
	    }
	    instance instance {
	        url url
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        token token
	        version version
	        verify-peer true|false
	        verify-host true|false
	        ca-cert /path/to/ca-cert
	        header header
	        proxy proxy-url
	        proxy-tunnel true|false
	        interval seconds
	        timeout seconds
	        metric-prefix prefix
	        label key value
	        query query
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **redfish** plugin was designed to collect data and metrics exposed by a
Redfish interface, which implements a structured REST API specified by the
[DMTF](https://www.dmtf.org/standards/redfish)
.

**query** *query*

> A **query** is identified by a unique key, and groups **resource** and
> **attribute** objects associated with a given endpoint.

> A **query** is shared by all the instances.

> For each query specified in the **query** field of a **instance**,
> there must be a matching **query**.

> **end-point** *url*

> > An **end-point** is a partial URI designated one specific resource
> > exposed by the Redfish interface to query.

> > This URI is qualified as "partial" since the root path of every resource
> > exposed by a Redfish interface (namely /redfish/v1 for the first
> > version of the protocol) should be omitted.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metrics name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple time in the **query** block.

> **resource** *resource*

> > A **resource** represents one composite object or an anonymous collection
> > of a resource exposed by a Redfish interface.

> > A **resource** should contain at least one **property**, but possibly
> > more than one.

> > **property** *property*

> > > A **property** represents one property of a composite object or of a
> > > member of an anonymous collection which should value should be collected
> > > as a metric.

> > > In the case of the property "Status", the metric type will be forced to
> > > gauge and the value will be 1 if "Health" is "OK" and "State" id "Enabled";
> > > otherwise it will be 0.

> > > **type** *counter|gauge*

> > > > The **type** that's used for each metric returned.
> > > > Must be *gauge*, *counter*, *info* or *unknown*.
> > > > If not set is unknown.
> > > > There must be exactly one **type** option inside each **property** block.

> > > **help** *help*

> > > > Set the **help** text for the metric.

> > > **metric** *metric*

> > > > Set the metric name.

> > > **label** *key* *value*

> > > > Append the label *key*=*value* to the submitting metrics.
> > > > Can appear multiple time in the **property** block.

> > > **label-from** *key* *attribute*

> > > > Specifies the attributes whose values will be used to create the labels.

> > > **scale** *value*

> > > > The values of metrics are multiplied by *value*.

> > > **shift** *value*

> > > > The values of metrics are added or subtracted by *value*.

> > > **select-id** *id* ...

> > > > If the "item" associated with the **property** is an anonymous collection,
> > > > only the members which IDs belong to the specified list are selected.
> > > > Note that the IDs start at 0.

> > > > If the "item" associated with the **property** is a composite object,
> > > > this field is ignored.

> > > **select-attr** *attribute* ...

> > > > If the "item" associated with the **property** is an anonymous collection,
> > > > only the members which exhibit attributes which names belong to the specified
> > > > list are selected.
> > > > In other words, this field makes it possible to select members of anonymous
> > > > collections based on a set of attributes they must have.

> > > > If the "item" associated with the **property** is a composite object,
> > > > this field is ignored.

> > > **select-attr-value** *key* *value*

> > > > If the "item" associated with the **property** is an anonymous collection,
> > > > only the members which exhibit an attribute which name is *key* and
> > > > value is *value*.
> > > > In other words, this field makes it possible to select members of anonymous
> > > > collections based on the fact that they have a specific attribute,
> > > > which is equal to a specific value.
> > > > Note that it is possible to specify several **select-attr-value** fields
> > > > so as to base the member selection on several attributes.

> > > > If the "item" associated with the **property** is a composite object,
> > > > this field is ignored.

> **attribute** *attribute*

> > An **attribute** represents one attribute of a resource exposed by a Redfish
> > interface.

> > In the case of the property "Status", the metric type will be forced to
> > gauge and the value will be 1 if "Health" is "OK" and "State" id "Enabled";
> > otherwise it will be 0.

> > **type** *counter|gauge*

> > > The **type** that's used for each metric returned.
> > > Must be *gauge*, *counter*, *info* or *unknown*.
> > > If not set is unknown.
> > > There must be exactly one **type** option inside each **attribute** block.

> > **help** *help*

> > > Set the **help** text for the metric.

> > **metric** *metric*

> > > Set the metric name.

> > **label** *key* *value*

> > > Append the label *key*=*value* to the submitting metrics.
> > > Can appear multiple time in the **property** block.

> > **label-from** *key* *attribute*

> > > Specifies the attributes whose values will be used to create the labels.

> > **scale** *value*

> > > The values of metrics are multiplied by *value*.

> > **shift** *value*

> > > The values of metrics are added or subtracted by *value*.

**instance** *instance*

> A **instance** groups the information associated with a host exhibiting
> a Redfish interface to query.
> Each declared **instance** should be identified by a unique key.

> If a **user** field is specified, a **passwd** field must be specified too.

> Several instances can be specified.
> If there is none, nothing will be collected.

> **url** *url*

> > **url** is the url of the node exhibiting a Redfish interface to query.

> **user** *username*

> > **user** is the username of an account with appropriate privileges to
> > submit the intended queries to the concerned Redfish interface.

> **user-env** *env-name*

> > Get the username to use if authorization is required from the environment
> > variable *env-name*.

> **password** *password*

> > **password** is the password of an account with appropriate privileges
> > to submit the intended queries to the concerned Redfish interface.

> **password-env** *env-name*

> > Get the password to use if authorization is required from the environment
> > variable *env-name*.

> **token** *token*

> > **token** is the X-Auth token associated with a session opened
> > on the target Redfish interface.

> **version** *version*

> > Version of redfish protocol to build the request path (/redfish/v1).
> > The default value is v1.

> **verify-peer** *true|false*

> > Enable or disable peer SSL certificate verification.
> > See
> > [http://curl.haxx.se/docs/sslcerts.html](http://curl.haxx.se/docs/sslcerts.html)
> > for details.
> > Enabled by default.

> **verify-host** *true|false*

> > Enable or disable peer host name verification.
> > If enabled, the plugin checks if the Common Name or a
> > Subject Alternate Name field of the SSL certificate matches the
> > host name provided by the **url** option.
> > If this identity check fails, the connection is aborted.
> > Obviously, only works when connecting to a SSL enabled server.
> > Enabled by default.

> **ca-cert** */path/to/ca-cert*

> > File that holds one or more SSL certificates.
> > If you want to use HTTPS you will possibly need this option.
> > What CA certificates come bundled with **libcurl** and are checked by
> > default depends on the distribution you use.

> **proxy** *proxy-url*

> > Set the proxy URL to send requests through the HTTP proxy.

> **proxy-tunnel** *true|false*

> > If set to *true* tunnel all operations through the HTTP proxy.

> **interval** *seconds*

> > Sets the interval (in seconds) in which the values will be collected from this
> > instanced.
> > By default the global **interval** setting will be used.

> **timeout** *seconds*

> > The **timeout** option sets the overall timeout for HTTP requests
> > to **url**, in seconds.
> > By default, the configured **interval** is used to set the timeout.
> > If **timeout** is bigger than the **interval**, keep in mind that each slow
> > network connection will stall one read thread.
> > Adjust the **read-threads** global setting accordingly to prevent this from
> > blocking other plugins.

> **metric-prefix** *prefix*

> > Prepends *prefix* to the metric name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times in the **instance** block.

> **query** *query*

> > **query** should contain the space-separated list of unique keys associated
> > with the queries to be submitted to the concerned Redfish interface.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

