NCOLLECTD-PROXYSQL(5) - File Formats Manual

# NAME

**ncollectd-proxysql** - Documentation of ncollectd's proxysql plugin

# SYNOPSIS

	load-plugin proxysql
	plugin proxysql {
	    instance name {
	        host hostname
	        user username
	        user-env env-name
	        password password
	        password-env env-name
	        port port
	        socket socket-path
	        ssl-key "/path/to/key.pem"
	        ssl-cert "/path/to/cert.pem"
	        ssl-ca "/path/to/ca.pem"
	        ssl-ca-path "/path/to/cas/"
	        ssl-cipher cyphers
	        connect-timeout seconds
	        interval seconds
	        label key value
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **proxysql** plugin requires **mysqlclient** to be installed.
It connects to one or more proxysql server when started and keeps the
connection up as long as possible.
When the connection is interrupted for whatever reason it will try
to re-connect.
The plugin will complain loudly in case anything goes wrong.

The configuration of the **proxysql** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.

**host** *hostname*

> Hostname of the proxsql server.
> Defaults to **localhost**.

**user** *username*

> Username to use when connecting to the proxysql.
> The user does not have to be granted any privileges
> (which is synonym to granting the USAGE privilege), unless you
> want to collect replication statistics.

**user-env** *env-name*

> Get the username to use when connecting to the proxysql from the
> environment variable *env-name*.

**password** *password*

> Password needed to log into the proxysql.

**password-env** *env-name*

> Get the password needed to log into the proxysql from the environment
> variable *env-name*.

**port** *port*

> TCP-port to connect to.
> If **host** is set to **localhost** (the default), this setting
> has no effect.
> See the documentation for the proxysql\_real\_connect function
> for details.

**socket** *socket-path*

> Specifies the path to the UNIX domain socket of the proxysql server.
> This option only has any effect, if **host** is set to **localhost**
> (the default).
> Otherwise, use the **port** option above.

**ssl-key** */path/to/key.pem*

> If provided, the X509 key in PEM format.

**ssl-cert** */path/to/cert.pem*

> If provided, the X509 cert in PEM format.

**ssl-ca** */path/to/ca.pem*

> If provided, the CA file in PEM format (check OpenSSL docs).
> If **ssl-ca** or **ssl-ca-path** is set, the connection must be
> encrypted and the server certificate must be signed by one of these
> CA certificates, otherwise the connection fails.
> With MariaDB Connector/C the server host name is verified as well.

**ssl-ca-path** */path/to/cas/*

> If provided, the CA directory (check OpenSSL docs).
> See **ssl-ca** for the verification of the server certificate.

**ssl-cipher** *cyphers*

> If provided, the SSL cipher to use.

**connect-timeout** *seconds*

> Sets the connect timeout for the client.
> Reading from and writing to the server time out after the
> collection **interval**.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this proxysql server.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

