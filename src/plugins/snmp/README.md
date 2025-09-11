NCOLLECTD-SNMP(5) - File Formats Manual

# NAME

**ncollectd-snmp** - Documentation of ncollectd's snmp plugin

# SYNOPSIS

	load-plugin snmp
	plugin snmp {
	    table table {
	        type counter|gauge
	        help help
	        metric metric
	        label key value
	        label-from key OID
	        label-suffix key
	        value OID
	        shift value
	        scale value
	        count true|false
	        filter-oid OID
	        filter-value  [incl|include|excl|exclude] value
	    }
	    data data {
	        type counter|gauge
	        help help
	        metric metric
	        label key value
	        label-from key OID
	        value OID
	        shift value
	        scale value
	    }
	    host host {
	        address ip-address|hostname
	        community community
	        version 1|2|3
	        timeout seconds
	        retries integer
	        interval seconds
	        username username
	        auth-protocol md5|sha
	        privacy-protocol aes|des
	        auth-passphrase passphrase
	        privacy-passphrase passphrase
	        security-level authpriv|authnopriv|noauthnopriv
	        local-cert fingerprint|fileprefix
	        peer-cert fingerprint|fileprefix
	        trust-cert fingerprint|fileprefix
	        peer-hostname hostname
	        context context
	        bulk-size integer
	        metric-prefix metric-prefix
	        label key value
	        collect data|table [data|table ...]
	    }
	}

# DESCRIPTION

The snmp plugin queries other hosts using SNMP, the simple network
management protocol, and translates the value it receives to ncollectd's
internal format and dispatches them.
Depending on the write plugins you have loaded they may be written to disk
or submitted to another instance or whatever you configured.

Because querying a host via SNMP may produce a timeout the "complex reads"
polling method is used.
The **read-threads** parameter in the main configuration influences the
number of parallel polling jobs which can be undertaken.
If you expect timeouts or some polling to take a long time, you should
increase this parameter.
Note that other plugins also use the same threads.

Since the aim of the **snmp** plugin is to provide a generic interface
to SNMP, its configuration is not trivial and may take some time.

Since the Net-SNMP library is used you can use all the environment
variables that are interpreted by that package.
See
snmpcmd(1)
for more details.

There are three types of blocks that can be contained in the **snmp** plugin
block: **table**, **data**, and **host**:

**table** *table*

> The **table** block defines a list of table of values that are
> to be queried.
> The following options can be set:

> **count** *true|false*

> > Instead of dispatching one value per table entry containing the *OID* given
> > in the **value** option, just dispatch a single count giving the
> > number of entries that would have been dispatched.
> > This is especially useful when combined with the filtering options (see below)
> > to count the number of entries in a Table matching certain criteria.

> > When **table** is used, the OIDs given to **values**,
> > **label-from** **filter-ioid** (see below) are queried using the
> > GETNEXT SNMP command until the subtree is left.
> > After all the lists (think: all columns of the table) have been read,
> > either (**count** set to *false*) **several** value sets will
> > be dispatched or (**count** set to *true*) one single value will
> > be dispatched.

> > For example, if you want to query the number of users on a system, you can use
> > HOST-RESOURCES-MIB::hrSystemNumUsers.0.
> > This is one value and belongs to one value list, therefore you  must use
> > **data** block.
> > Please note that, in this case, you have to include the sequence number
> > (zero in this case) in the OID.

> > Counter example: If you want to query the interface table provided by the
> > IF-MIB, e.g. the bytes transmitted.
> > There are potentially many interfaces, so you will want to user is a **table**
> > block.

> **type** *counter|gauge|unknown*

> > The **type** that's used for each value read.
> > Must be *gauge*, *counter* or *unknown*.
> > If not set is unknown.

> **help** *help*

> > Set the **help** text for the metric.

> **metric** *metric*

> > Set the metric name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times.

> **label-from** *key* *OID*

> > Append the label with *key* and value from the queried *OID*.
> > The *OID* is interpreted as an SNMP-prefix that will return a list
> > of values.
> > An example would be the IF-MIB::ifDescr subtree.
> > variables(5)
> > from the SNMP distribution describes the format of OIDs.

> **label-suffix** *key*

> > Append the label with *key* and the *value*&zwnj;** the suffix from**
> > the queried *OID*.

> **value** *OID*

> > Configures the values to be queried from the SNMP host.
> > variables(5)
> > from the SNMP distribution describes the format of OIDs.

> > The *OID* must be the prefix of all the values to query, e.g.
> > IF-MIB::ifInOctets for all the counters of incoming traffic.
> > This subtree is walked (using GETNEXT) until a value from outside the
> > subtree is returned.

> **shift** *value*

> > *Value* is added to gauge-values returned by the SNMP-agent after they have
> > been multiplied by any **scale** value.
> > If, for example, a thermometer returns degrees Kelvin you could specify a shift
> > of **273.15** here to store values in degrees Celsius.
> > The default value is, of course, **0.0**.

> > This value is not applied to counter-values.

> **scale** *value*

> > The gauge-values returned by the SNMP-agent are multiplied by  *value*.
> > This is useful when values are transferred as a fixed point real number.
> > For example, thermometers may transfer **243** but actually mean **24.3**,
> > so you can specify a scale value of **0.1** to correct this.
> > The default value is, of course, **1.0**.

> > This value is not applied to counter-values.

> **filter-oid** *OID*

> **filter-value** \[*incl|include|excl|exclude*] *value*

> > When **table** is set to *true*, these options allow to configure
> > filtering based on MIB values.

> > The **filter-oid** declares *OID* to fill table column with values.
> > The **filter-value** declares values to do match.
> > Whether table row will be collected or ignored depends on the
> > **filter-value** setting.
> > As with other plugins that use the daemon's include/exclude functionality,
> > a string that starts and ends with a slash is interpreted as a
> > regular expression.

> > If no selection is configured at all, **all** table rows are selected.

**data** *data*

> The **data** block defines a list of values or a table of values that are
> to be queried.
> The OID given to **value** (see below) are queried using the NGET
> SNMP command (see
> snmpget(1)
> ) and transmitted to mcollectd.
> The following options can be set:

> **type** *counter|gauge|unknown*

> > The **type** that's used for each value read.
> > Must be *gauge*, *counter* or *unknown*.
> > If not set is unknown.

> **help** *help*

> > Set the **help** text for the metric.

> **metric** *metric*

> > Set the metric name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times.

> **label-from** *key* *OID*

> > Append the label with *key* and value from the queried *OID*.

> **value** *OID*

> > Configures the values to be queried from the SNMP host.
> > variables(5)
> > from the SNMP distribution describes the format of OIDs.

> > The *OID* must be the OID of exactly one value, e.g.
> > IF-MIB::ifInOctets.3 for the third counter of incoming traffic.

> **shift** *value*

> > *Value* is added to gauge-values returned by the SNMP-agent after they have
> > been multiplied by any **scale** value.
> > If, for example, a thermometer returns degrees Kelvin you could specify a shift
> > of **273.15** here to store values in degrees Celsius.
> > The default value is, of course, **0.0**.

> > This value is not applied to counter-values.

> **scale** *value*

> > The gauge-values returned by the SNMP-agent are multiplied by  *value*.
> > This is useful when values are transferred as a fixed point real number.
> > For example, thermometers may transfer **243** but actually mean **24.3**,
> > so you can specify a scale value of **0.1** to correct this.
> > The default value is, of course, **1.0**.

> > This value is not applied to counter-values.

**host** *host*

> The **host** block defines which hosts to query, which SNMP community and
> version to use and which of the defined **data** to query.

> **address** *ip-address|hostname*

> > Set the address to connect to.
> > Address may include transport specifier and/or port number.

> **community** *community*

> > Pass *community* to the host. (ignored for SNMPv3).

> **version** *1|2|3*

> > Set the SNMP version to use.
> > When giving *2* version *2c* is actually used.

> **timeout** *seconds*

> > How long to wait for a response.
> > The Net-SNMP library default is 1 second.

> **retries** *integer*

> > The number of times that a query should be retried after the timeout expires.
> > The Net-SNMP library default is 5.

> **interval** *seconds*

> > Collect data from this host every *seconds* seconds.
> > This option is meant for devices with not much CPU power, e.g. network
> > equipment such as switches, embedded devices, rack monitoring systems and so on.

> **username** *username*

> > Sets the *username* to use for SNMPv3 User-based Security Model
> > (USM) security.

> **auth-protocol** *md5|sha*

> > Selects the authentication protocol for SNMPv3 User-based Security Model
> > (USM) security.

> **privacy-protocol** *aes|des*

> > Selects the privacy (encryption) protocol for SNMPv3 User-based Security Model
> > (USM) security.

> **auth-passphrase** *passphrase*

> > Sets the authentication passphrase for SNMPv3 User-based Security Model
> > (USM) security.

> **privacy-passphrase** *passphrase*

> > Sets the privacy (encryption) passphrase for SNMPv3 User-based Security Model
> > (USM) security.

> **security-level** *authpriv|authnopriv|noauthnopriv*

> > Selects the security level for SNMPv3 User-based Security Model (USM) security.

> **local-cert** *fingerprint|fileprefix*

> > Sets the fingerprint or the filename prefix of the local certificate,
> > key, and (if supported) intermediate certificates for SNMPv3 Transport
> > Security Model (TSM) security.

> **peer-cert** *fingerprint|fileprefix*

> > Sets the fingerprint or the filename prefix of the self signed remote peer
> > certificate to be accepted as presented by the SNMPv3 server for SNMPv3
> > Transport Security Model (TSM) security.

> **trust-cert** *fingerprint|fileprefix*

> > Sets the fingerprint or the filename prefix of the certificate authority
> > certificates to be trusted by ncollectd-snmp for SNMPv3 Transport Security
> > Model (TSM) security.
> > This option can only be specified once.
> > From Net-SNMP v5.10 onwards, all certificates in files matching the
> > given filename prefix are trusted.

> **peer-hostname** *hostname*

> > If specified, the hostname of the SNMPv3 server will be checked against the
> > peer certificate presented by the SNMPv3 server.

> **context** *context*

> > Sets the *context* for SNMPv3 security.

> **bulk-size** *integer*

> > Configures the size of SNMP bulk transfers.
> > The default is 0, which disables bulk transfers altogether.

> **metric-prefix** *metric-prefix*

> > Prepends *prefix* to the metric name in the **data** block.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.
> > Can appear multiple times.

> **collect** *data* \[*data* ...]

> > Defines which values to collect. *data* refers to one of the
> > **data** block above.
> > Since the config file is read top-down you need to define the data before
> > using it here.

# SECURITY

SNMP provides various security levels, ranging from open SNMPv1 and SNMPv2c,
to the secure SNMPv3 User-based Security Model (USM) and Transport Security
Model (TSM) options.

## SNMPv1 / SNMPv2c Security

When **version** 1 or 2 is used, anyone with knowledge of the community
string can connect to the SNMP server.

No authentication or privacy is supported in these modes.

## SNMPv3 User-based Security Model (USM) Security

When **address** prefixes such as *udp:* or *udp6:* are used
along with **version** 3 and the **username** option, USM security
is enabled.

Security in this mode is based on shared secrets, and can offer
optional authentication and privacy.

The digest and encryption algorithms specified by **auth-protocol** and
**privacy-protocol** must match those on the SNMPv3 server.

The user credentials used by the SNMPv3 server are specified by the
**username** option.

## SNMPv3 Transport Security Model (TSM) Security

When TLS/DTLS **address** prefixes such as *dtlsudp:* or *dtlsudp6:*
are used along with the **local-cert** option, TSM security is enabled.

Security in this mode is based on X509 certificates and public/private keys.
The SNMPv3 server and ncollectd-snmp client authenticate and secure the
connection through server and client certificates.
The SNMPv3 server will decide the user credentials to be applied based on
the attributes of the client certificate presented by ncollectd-snmp in
**local-cert**.

The certificates and keys are stored in any of the series of certificate
store paths supported by the Net-SNMP library, and are scanned and
indexed for performance.
The path cannot be specified directly via ncollectd-snmp.

Certificates are chosen by specifying the fingerprint of the certificate
or the name prefix of the file the certificate is stored in.
The algorithm used for the fingerprint matches the algorithm used to sign
the certificate.

Files containing keys must have no group or world permissions, otherwise the
contents of the files will be silently ignored.

If a filename prefix is used, certificates are picked up from files with
specific prefixes known to Net-SNMP matching the filename prefix.
This value is not a path.
For example, if a filename prefix of "router-cert" is specified, files called
*router-cert.pem*, *router-cert.crt*, *router-cert.cer*,
*router-cert.cert*, *router-cert.der*, *router-cert.key* and
*router-cert.private* will be scanned for certificates and keys.

The Net-SNMP library v5.9 and older has limited support for certificates
other than self signed certificates.
Intermediate certificates are ignored by these older versions of
Net-SNMP, and only the first certificate in each file is recognised.
Net-SNMP v5.10 and higher recognise concatenated intermediate
certificates in files, as well as multiple CA certificates specified in
one file, such as the *tls-ca-bundle.pem* available on many platforms.
This allows certificates to be used that have been provided by a
PKI, either privately or through a public certificate authority.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd @NCOLLECTD\_VERSION@ - @NCOLLECTD\_DATE@
