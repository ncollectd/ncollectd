NCOLLECTD-NOTIFY\_SNMP(5) - File Formats Manual

# NAME

**ncollectd-notify\_snmp** - Documentation of ncollectd's notify\_snmp plugin

# SYNOPSIS

	load-plugin notify_snmp
	plugin notify_snmp {
	    oids oids-name {
	        enterprise-oid oid
	        trap-oid oid
	        name-oid oid
	        timeStamp-oid oid
	        severity-oid oid
	        labels-oid oid
	        annotations-oid oid
	    }
	    target target {
	        address address
	        version [1|2]
	        community community
	        session-reuse session-reuse
	        oids oids-name
	    }
	}

# DESCRIPTION

The **notify\_snmp** plugin use the Net-SNMP library to send notifications as
*SNMP Traps*.

The plugin's configuration consists of a number of **oids** and
**target** blocks.

**oids** *oids-name*

> The **oids** block configure the list of object identifiers to send in the
> *SNMP Trap*.
> There are sent in the order show.
> They are identified by the name that is given in the opening line of the block.

> **enterprise-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the Enterprise *oid*, used in SNMP version 1.

> **trap-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the Trap *oid*, used in SNMP version 2.

> **name-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the *oid* for the notification name.

> **timeStamp-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the *oid* for the notification timestamp.

> **severity-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the *oid* for the notification severity.

> **labels-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the *oid* for the notification labels.

> **annotations-oid**&zwnj;** **&zwnj;*oid*&zwnj;**&zwnj;**

> > Set the *oid* for the notification annotations.

**target** *target*

> The **target** block configure the destination of the *SNMP Trap* and
> which OIDs should be send in the trap.
> They are identified by the name that is given in the opening line of the block.

> **address** **address**

> > Set the address of the target to connect to.
> > The default protocol is udp and the port is 161.
> > See the
> > snmpcmd(1)
> > manual page for more details.

> **version** \[*1|2*]

> > Set the SNMP version to use.
> > When giving **2** version **2c** is actually used.
> > Version 3 is not supported by this plugin.
> > TODO

> **community** *community*

> > Pass the *community* to the target.

> **session-reuse** *session-reuse*

> > Keep SNMP session open between notifications send.

> **oids** *oids-name*

> > Defines which OIDs are used in the Trap. *name* refers to one of the
> > **oids** block above.
> > Since the config file is read top-down you need to define the data
> > before using it here.

> > If option **oids** is not specified the default values defined in the plugin
> > are used.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
