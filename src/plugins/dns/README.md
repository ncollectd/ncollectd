NCOLLECTD-DNS(5) - File Formats Manual

# NAME

**ncollectd-dns** - Documentation of ncollectd's dns plugin

# SYNOPSIS

	load-plugin dns
	plugin dns {
	    instance instance {
	        server server
	        domain domain
	        udp-port port
	        tcp-port port
	        use-vc true|false
	        use-tcp true|false
	        primary true|false
	        ignore-truncated true|false
	        recurse true|false
	        search true|false
	        aliases true|false
	        edns true|false
	        edns-size size
	        resolvconf-path path-to-resolv.conf
	        timeout seconds
	        tries tries
	        ndots ndots
	        rotate true|false
	        label key value
	        interval seconds
	        query query {
	            class IN|CHAOS|HS|ANY
	            type A|AAAA|NS|MX|...
	            convert-ptr true|false
	            convert-ptr-bit-string true|false
	            label key value
	            validate expression
	        }
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **dns** plugin generare metrics with dns queries.

The following options are accepted within each **instance** block:

**server** *server*

> The servers to contact, instead of the servers specified in resolv.conf
> or the local named.

**domain** *domain*

> The domains to search, instead of the domains specified in resolv.conf
> or the domain derived from the kernel hostname variable.

**udp-port** *port*

> The port to use for queries over UDP, in network byte order.
> The default value is 53, the standard name service port.

**tcp-port** *port*

> The port to use for queries over TCP, in network byte order.
> The default value is 53, the standard name service port.

**use-vc** *true|false*

**use-tcp** *true|false*

> Always use TCP queries (the "virtual circuit") instead of UDP queries.
> Normally, TCP is only used if a UDP query yields a truncated result.

**primary** *true|false*

> Only query the first server in the list of servers to query.

**ignore-truncated** *true|false*

> If a truncated response to a UDP query is received, do not fall back to TCP;
> simply continue on with the truncated response.

**recurse** *true|false*

> If false do not set the "recursion desired" bit on outgoing queries,
> so that the name server being contacted will not try to fetch the answer
> from other servers if it doesn't know the answer locally.
> Default is true.

**search** *true|false*

> If false do not use the default search domains; only query hostnames as-is or
> as aliases.
> Default is true.

**aliases** *true|false*

> If false do not honor the HOSTALIASES environment variable, which normally
> specifies a file of hostname translations.
> Default is true.

**edns** *true|false*

> Include an EDNS pseudo-resource record (RFC 2671) in generated requests.

**edns-size** *size*

> The message size to be advertized in EDNS.

**resolvconf-path** *path-to-resolv.conf*

> The path to use for reading the resolv.conf file.
> The default is /etc/resolv.conf

**timeout** *seconds*

> The number of seconds (with a resolution of milliseconds) each name server is
> given to respond to a query on the first try.
> (After the first try, the timeout algorithm becomes more complicated,
> but scales linearly with the value of timeout.)
> The default is five seconds.

**tries** *tries*

> The number of tries the resolver will try contacting each name server before
> giving up.
> The default is four tries.

**ndots** *ndots*

> The number of dots which must be present in a domain name for it to be
> queried for "as is" prior to querying for it with the default domain
> extensions appended.
> The default value is 1 unless set otherwise by resolv.conf or the RES\_OPTIONS
> environment variable.

**rotate** *true|false*

> If true perform round-robin selection of the nameservers configured for
> each resolution, if false always use the list of nameservers in the same order.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **instance** block.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**query** *query*

> **class** *IN|CHAOS|HS|ANY*

> > IN CHAOS HS ANY

> **type** *A|AAAA|NS|MX|...*

> > A NS MD MF CNAME SOA MB MG MR NULL WKS PTR HINFO MINFO MX TXT RP AFSDB X25
> > ISDN RT NSAP NSAP\_PTR SIG KEY PX GPOS AAAA LOC SRV AXFR MAILB MAILA NAPTR
> > DS SSHFP RRSIG NSEC DNSKEY CAA URI ANY

> **convert-ptr** *true|false*

> **convert-ptr-bit-string** *true|false*

> **label** *key* *value*

> **validate** *expression*

> > **response.id**

> > **response.flags.qr**

> > **response.flags.aa**

> > **response.flags.tc**

> > **response.flags.rd**

> > **response.flags.ra**

> > **response.rtime**

> > **response.rcode**

> > **response.opcode**

> > **response.query.length**

> > > **name**

> > > **type**

> > > **class**

> > **response.answer.length**

> > **response.authority.lenth**

> > **response.additional.length**

> > > **name**

> > > **type**

> > > **class**

> > > **ttl**

> > > **cname.name**

> > > **mb.name**

> > > **md.name**

> > > **mf.name**

> > > **mg.name**

> > > **mr.name**

> > > **ns.name**

> > > **ptr.name**

> > > **hinfo.hardware**

> > > **hinfo.os**

> > > **minfo.mailbox**

> > > **minfo.error\_mailbox**

> > > **mx.priority**

> > > **mx.mailserver**

> > > **soa.master**

> > > **soa.responsible**

> > > **soa.serial**

> > > **soa.refresh\_interval**

> > > **soa.retry\_interval**

> > > **soa.expire**

> > > **soa.negative\_caching\_ttl**

> > > **txt.**

> > > **caa.**

> > > **a.address**

> > > **aaaa.address**

> > > **srv.priority**

> > > **srv.weight**

> > > **srv.port**

> > > **srv.target**

> > > **uri.priority**

> > > **uri.weight**

> > > **uri.target**

> > > **naptr.order**

> > > **naptr.preference**

> > > **naptr.flags**

> > > **naptr.service**

> > > **naptr.regex**

> > > **naptr.replacement**

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
