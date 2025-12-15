NCOLLECTD-WRITE\_EXPORTER(5) - File Formats Manual

# NAME

**ncollectd-write\_exporter** - Documentation of ncollectd's write\_exporter plugin

# SYNOPSIS

	load-plugin write_exporter
	plugin write_exporter {
	    instance "instance" {
	        host host
	        port port
	        staleness-delta seconds
	        private-key file-path
	        private-key-password password
	        certificate
	        tls-priority
	        auth-method basic|digest
	        user user
	        user-env env-name
	        password password
	        password-env env-name
	        realm realm
	        format-metric influxdb|graphite|json|kairosdb|opentsdb|openmetrics|opentelemetry|remote
	    }
	}

# DESCRIPTION

The **write\_exporter** plugin implements a tiny webserver that can be scraped.

The following options are accepted within each **instance** block:

**host** *host*

> Bind to the hostname / address *host*.
> By default, the plugin will bind to the "any" address, i.e. accept packets sent
> to any of the hosts addresses.

> This option is supported only for libmicrohttpd newer than 0.9.0.

**port** *port*

> Port the embedded webserver should listen on.
> Defaults to B9103.

**staleness-delta** *seconds*

> Time in seconds after considers a metric "stale" if it hasn't seen any
> update for it.
> It defaults to 300 seconds (5 minutes).

**private-key** *file-path*

**private-key-password** *password*

**certificate**

**tls-priority**

**auth-method** *basic|digest*

**user** *user*

**user-env** *env-name*

**password** *password*

**password-env** *env-name*

**realm** *realm*

**format-metric** *influxdb|graphite|json|kairosdb|opentsdb|openmetrics|remote*

> Selects the format in which metrics are served.

> **influxdb** \[*sec|msec|usec|nsec*]

> **graphite**

> **json**

> **kairosdb** \[*telnet \[sec|msec]|json*]

> **opentsdb** \[telnet|json]

> **openmetrics** \[text|protob]

> **opentelemetry** \[json]

> **remote** \[metadata]

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

