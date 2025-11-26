NCOLLECTD-JOLOKIA(5) - File Formats Manual

# NAME

**ncollectd-jolokia** - Documentation of ncollectd's jolokia plugin

# SYNOPSIS

	load-plugin jolokia
	plugin jolokia {
	    mbeans name {
	        mbean name {
	            path path
	            metric-prefix prefix
	            label key value
	            label-from key value-from
	            attribute name {
	                metric metric
	                type gauge|counter
	                label key value
	                label-from key value-from
	                help help
	            }
	        }
	    }
	    instance name {
	        url url
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        verify-peer true|false
	        verify-host true|false
	        ca-cert /path/to/ca
	        header header
	        timeout seconds
	        label key value
	        interval seconds
	        metric-prefix prefix
	        collect mbeans-name ...
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **jolokia** plugin collects values from MBeanServevr - servlet engines
equipped with the jolokia (
[https://jolokia.org](https://jolokia.org)
) MBean.
It sends a pre-configured JSON-Postbody to the servlet via HTTP commanding
the jolokia Bean to reply with a single JSON equipped with all JMX counters
requested.
By reducing TCP roundtrips in comparison to conventional JMX clients that
query one value via tcp at a time, it can return hundreds of values in one
roundtrip.
Moreof - no java binding is required in collectd to do so.

**url** *url*

**user** *user*

> Username to use if authorization is required to read the page.

**user-env** *env-name*

> Get the username to use if authorization is required to read the page from the
> environment variable *env-name*

**password** *password*

> Password to use if authorization is required to read the page.

**password-env** *env-name*

> Get the Password to use if authorization is required to read the page from the
> environment variable *env-name*

**verify-peer** *true|false*

> Enable or disable peer SSL certificate verification.
> See
> [http://curl.haxx.se/docs/sslcerts.html](http://curl.haxx.se/docs/sslcerts.html)
> for details.
> Enabled by default.

**verify-host** *true|false*

> Enable or disable peer host name verification.
> If enabled, the plugin checks if the Common Name or a
> Subject Alternate Name field of the SSL certificate matches the host
> name provided by the **url** option.
> If this identity check fails, the connection is aborted.
> Obviously, only works when connecting to a SSL enabled server.
> Enabled by default.

**ca-cert** */path/to/ca*

> File that holds one or more SSL certificates.
> If you want to use HTTPS you will possibly need this option.
> What CA certificates come bundled with **libcurl**
> and are checked by default depends on the distribution you use.

**header** *header*

> A HTTP header to add to the request.
> Multiple headers are added if this option
> is specified more than once.

**timeout** *seconds*

> The **timeout** option sets the overall timeout for HTTP requests
> to **url**, in seconds.
> By default, the configured **interval** is used to set the timeout.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple times.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> URL.
> By default the global **interval** setting will be used.

**metric-prefix** *prefix*

> Prepends *prefix* to the metric name.

**collect** *mbeans-name* ...

> Associates the mbeans named with this jolokia instance.
> The mbeans needs to be defined before this statement, i. e. all mbeans block
> you want to refer to, must be placed above the instance block you want to
> refer to them from.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

**mbeans** *name*

> Mbeans block define a list of mbeans and how the returned data should be
> interpreted.
> The name of the the mbeans block needs to be unique.

> **mbean** *name*

> > One **mbean** block configures the translation of the gauges of one bean
> > to their respective collectd names, where *name* sets the main name.

> > **path** *path*

> > **metric-prefix** *prefix*

> > > Prepends *prefix* to the metric name.

> > **label** *key* *value*

> > > Append the label *key*=*value* to the submitting metrics.
> > > Can appear multiple times.

> > **label-from** *key* *value-from*

> > **attribute** *name*

> > > A bean can contain several attributes.
> > > Each one can be matched by a attribute block or be ignored.

> > > **metric** *metric*

> > > **type** *gauge|counter*

> > > **label** *key* *value*

> > > > Append the label *key*=*value* to the submitting metrics.
> > > > Can appear multiple times.

> > > **label-from** *key* *value-from*

> > > **help** *help*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

