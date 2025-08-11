NCOLLECTD-JAVA(5) - File Formats Manual

# NAME

**ncollectd-java** - Documentation of ncollectd's java plugin

# SYNOPSIS

	load-plugin java
	plugin java {
	    jvm-arg "arg"
	    load-plugin "java-class"
	    plugin "name" {
	        # To be parsed by the plugin
	    }
	}

# DESCRIPTION

The **java** plugin embeds a *Java Virtual Machine* (JVM) into
**ncollectd** and provides a Java interface to part of ncollectd's API.
This makes it possible to write additions to the daemon in Java.

Example:

	plugin "java" {
	    jvm-arg "-verbose:jni"
	    jvm-arg "-Djava.class.path=/opt/ncollectd/lib/ncollectd/bindings/java"
	    load-plugin "org.ncollectd.java.Foobar"
	    plugin "org.ncollectd.java.Foobar" {
	        # To be parsed by the plugin
	    }
	}

Available configuration options:

**jvm-arg** *arg*

> Argument that is to be passed to the *Java Virtual Machine* (JVM).
> This works exactly the way the arguments to the *java* binary on the
> command line work.
> Execute java --help for details.

> Please note that **all** these options must appear **before** (i. e. above)
> any other options! When another option is found, the JVM will be started and
> later options will have to be ignored!.

**load-plugin** *java-class*

> Instantiates a new *java-class* object.
> The constructor of this object very likely then registers one or more callback
> methods with the server.

> When the first such option is found, the virtual machine (JVM) is created.
> This means that all **jvm-arg** options must appear before (i. e. above) all
> **load-plugin** options!.

**plugin** *name*

> The entire block is passed to the Java plugin as an
> org.ncollectd.api.ConfigItem object.

> For this to work, the plugin has to register a configuration callback first,
> see.
> This means, that the **plugin** block must appear after the appropriate
> **load-plugin** block.
> Also note, that *name* depends on the (Java) plugin registering the
> callback and is completely independent from the *java-class* argument
> passed to **load-plugin**.

# OVERVIEW

When writing additions for ncollectd in Java, the underlying C base is mostly
hidden from you.
All complex data types are converted to their Java counterparts before
they're passed to your functions.
These Java classes reside in the org.ncollectd.api namespace.

The **java** plugin will create one object of each class configured with the
**load-plugin** option.
The constructor of this class can then register "callback methods",
i. e. methods that will be called by the daemon when appropriate.

The available classes are:

**org.ncollectd.api.NCollectd**

> All API functions exported to Java are implemented as static functions of this
> class.
> See **EXPORTED API FUNCTIONS** below.

**org.ncollectd.api.ConfigValue**

> Corresponds to config\_value\_t,
> defined in src/libconfig/config.h.

**org.ncollectd.api.ConfigItem**

> Corresponds to config\_item\_t,
> defined in src/libconfig/config.h.

**org.ncollectd.api.Notification**

> Corresponds to notification\_t,
> defined in src/libmetric/notification.h.

**org.ncollectd.api.MetricFamily**

> Corresponds to metric\_family\_t,
> defined in src/libmetric/metric.h.

**org.ncollectd.api.MetricUnknown**

> Corresponds to unknown\_t,
> defined in src/libmetric/metric.h.

**org.ncollectd.api.MetricGauge**

> Corresponds to gauge\_t,
> defined in src/libmetric/metric.h.

**org.ncollectd.api.MetricCounter**

> Corresponds to counter\_t,
> defined in src/libmetric/metric.h.

**org.ncollectd.api.MetricInfo**

> Corresponds to label\_set\_t,
> defined in src/libmetric/label\_set.h.

**org.ncollectd.api.MetricStateSet**

> Corresponds to state\_set\_t,
> defined in src/libmetric/state\_set.h.

**org.ncollectd.api.MetricHistogram**

> Corresponds to histogram\_t,
> defined in src/libmetric/histogram.h.

**org.ncollectd.api.MetricSummary**

> Corresponds to summary\_t,
> defined in src/libmetric/histogram.h.

In the remainder of this document, we'll use the short form of these names, for
example **Notification**.
In order to be able to use these abbreviated names, you need to **import**
the classes.

# EXPORTED API FUNCTIONS

All ncollectd API functions that are available to Java plugins are implemented
as *public static* functions of the **Collectd** class.
This makes calling these functions pretty straight forward.
For example, to send an error message to the daemon,
you'd do something like this:

	  NCollectd.logError ("That wasn't chicken!");

The following are the currently exported functions.

## registerConfig

Signature: *int* **registerConfig** (*String* name,
*NCollectdConfigInterface* object);

Registers the **config** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"config callback"** below.

## registerInit

Signature: *int* **registerInit** (*String* name,
*NCollectdInitInterface* object);

Registers the **init** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"init callback"** below.

## registerRead

Signature: *int* **registerRead** (*String* name,
*NCollectdReadInterface* object)

Registers the **read** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"read callback"** below.

## registerWrite

Signature: *int* **registerWrite** (*String* name,
*NCollectdWriteInterface* object)

Registers the **write** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"write callback"** below.

## registerShutdown

Signature: *int* **registerShutdown** (*String* name,
*NCollectdShutdownInterface* object);

Registers the **shutdown** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"shutdown callback"** below.

## registerLog

Signature: *int* **registerLog** (*String* name,
*NCollectdLogInterface* object);

Registers the **log** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"log callback"** below.

## registerNotification

Signature: *int* **registerNotification** (*String* name,
*NCollectdNotificationInterface* object);

Registers the **notification** function of *object* with the daemon.

Returns zero upon success and non-zero when an error occurred.

See **"notification callback"** below.

## dispatchMetricFamily

Signature: *int* **dispatchMetricFamily** (*MetricFamily* object)

Passes the metrics represented by the **MetricFamily** object to the
**plugin\_dispatch\_metric\_family** function of the daemon.

Returns zero upon success or non-zero upon failure.

## dispatchNotification

Signature: *int* **dispatchNotification** (*Notification* object);

Returns zero upon success or non-zero upon failure.

## logError

Signature: *void* **logError** (*String*)

Sends a log message with severity **ERROR** to the daemon.

## logWarning

Signature: *void* **logWarning** (*String*)

Sends a log message with severity **WARNING** to the daemon.

## logNotice

Signature: *void* **logNotice** (*String*)

Sends a log message with severity **NOTICE** to the daemon.

## logInfo

Signature: *void* **logInfo** (*String*)

Sends a log message with severity **INFO** to the daemon.

## logDebug

Signature: *void* **logDebug** (*String*)

Sends a log message with severity **DEBUG** to the daemon.

# REGISTERING CALLBACKS

When starting up, ncollectd creates an object of each configured class.
The constructor of this class should then register "callbacks" with the daemon,
using the appropriate static functions in **ncollectd**,
see **EXPORTED API FUNCTIONS** above.
To register a callback, the object being passed to one of the register
functions must implement an appropriate interface, which are all in the
**org.ncollectd.api** namespace.

A constructor may register any number of these callbacks, even none.
An object without callback methods is never actively called by ncollectd,
but may still call the exported API functions.
One could, for example, start a new thread in the constructor and dispatch
(submit to the daemon) values asynchronously, whenever one is available.

Each callback method is now explained in more detail:

## config callback

Interface: **org.ncollectd.api.NCollectdConfigInterface**

Signature: *int* **config** (*OConfigItem* ci)

This method is passed a **OConfigItem** object, if both, method and
configuration, are available.
**OConfigItem** is the root of a tree representing the configuration
for this plugin.
The root itself is the representation of the **Plugin** block,
so in next to all cases the children of the root are the first
interesting objects.

To signal success, this method has to return zero.
Anything else will be considered an error condition and the plugin will be
disabled entirely.

See **"registerConfig"** above.

## init callback

Interface: **org.ncollectd.api.NCollectdInitInterface**

Signature: *int* **init** ()

This method is called after the configuration has been handled.
It is supposed to set up the plugin. e. g. start threads, open connections,
or check if can do anything useful at all.

To signal success, this method has to return zero.
Anything else will be considered an error condition and the plugin will be
disabled entirely.

See **"registerInit"** above.

## read callback

Interface: **org.ncollectd.api.NCollectdReadInterface**

Signature: *int* **read** ()

This method is called periodically and is supposed to gather statistics in
whatever fashion.
These statistics are represented as a **MetricFamily** object and sent to
the daemon using **ddispatchMetricFamily**.

To signal success, this method has to return zero.
Anything else will be considered an error condition and cause an appropriate
message to be logged.
Currently, returning non-zero does not have any other effects.
In particular, Java "read"-methods are not suspended for increasing intervals
like C "read"-functions.

See **"registerRead"** above.

## write callback

Interface: **org.ncollectd.api.NCollectdWriteInterface**

Signature: *int* **write** (*MetricFamily* object)

This method is called whenever a value is dispatched to the daemon.
The corresponding C "write"-functions are passed a C&lt;data\_set\_t, so they can
decide which values are absolute values (gauge) and which are counter values.
To get the corresponding C&lt;ListE&lt;ltDataSourceE&lt;gt,
call the **getDataSource** method of the **ValueList** object.

To signal success, this method has to return zero.
Anything else will be considered an error condition and cause an appropriate
message to be logged.

See **"registerWrite"** above.

## shutdown callback

Interface: **org.ncollectd.api.NCollectdShutdownInterface**

Signature: *int* **shutdown** ()

This method is called when the daemon is shutting down.
You should not rely on the destructor to clean up behind the object but use
this function instead.

To signal success, this method has to return zero.
Anything else will be considered an error condition and cause an appropriate
message to be logged.

See **"registerShutdown"** above.

## log callback

Interface: **org.ncollectd.api.NCollectdLogInterface**

Signature: *void* **log** (*int* severity, *String* message)

This callback can be used to receive log messages from the daemon.

The argument *severity* is one of:

*	**org.ncollectd.api.Collectd.LOG\_ERR**

*	**org.ncollectd.api.Collectd.LOG\_WARNING**

*	**org.ncollectd.api.Collectd.LOG\_NOTICE**

*	**org.ncollectd.api.Collectd.LOG\_INFO**

*	**org.ncollectd.api.Collectd.LOG\_DEBUG**

The function does not return any value.

See **"registerLog"** above.

## notification callback

Interface: **org.ncollectd.api.NCollectdNotificationInterface**

Signature: *int* **notification** (*Notification* n)

This callback can be used to receive notifications from the daemon.

To signal success, this method has to return zero.
Anything else will be considered an error condition and cause an appropriate
message to be logged.

See **"registerNotification"** above.

# EXAMPLES

This short example demonstrates how to register a read callback with the
daemon:

	  import org.ncollectd.api.NCollectd;
	  import org.ncollectd.api.MetricFamily;
	  import org.ncollectd.api.MetricGauge;
	
	  import org.ncollectd.api.NCollectdReadInterface;
	
	  public class Foobar implements NCollectdReadInterface
	  {
	    public Foobar ()
	    {
	      NCollectd.registerRead ("Foobar", this);
	    }
	
	    public int read ()
	    {
	      MetricFamily fam = new MetricFamily(MetricFamily.METRIC_TYPE_GAUGE, "test");
	      fam.addMetric(new MetricGauge(10));
	
	      /* Do something... */
	
	      NCollectd.dispatchMetricFamily (fam);
	    }
	  }

# PLUGINS

The following plugins are implemented in *Java*.
Both, the **load-plugin** option and the **plugin** block must be inside
the **plugin** *java* block (see above).

## GenericJMX plugin

The GenericJMX plugin reads *Managed Beans* (MBeans) from an
*MBeanServer* using JMX.
JMX is a generic framework to provide and query various management information.
The interface is used by Java processes to provide internal statistics as well
as by the *Java Virtual Machine* (JVM) to provide information about the
memory used, threads and so on.

The configuration of the *GenericJMX plugin* consists of two blocks:
*MBean* blocks that define a mapping of MBean attributes to the &#8220;metrics&#8221;
used by **ncollectd**, and **Connection** blocks which define the
parameters needed to connect to an *MBeanServer* and what data to collect.
The configuration of the *SNMP plugin* is similar in nature,
in case you know it.

	plugin java {
	    jvm-arg "-Djava.class.path=/usr/share/ncollectd/java/ncollectd-api.jar:/usr/share/ncollectd/java/generic-jmx.jar"
	    load-plugin  "org.ncollectd.java.GenericJMX"
	    plugin GenericJMX {
	        mbean name {
	            object-name "pattern"
	            label key value
	            label-from key property
	            metrix-prefix "prefix"
	            metric name {
	                label key value
	                label-from key property
	                type unknown|gauge|counter
	                attribute attribute
	            }
	        }
	        connection {
	            service-url url
	            label key value
	            metric-prefix prefix
	            collect mbean
	            user user
	            password password
	        }
	    }
	}

**mbean**

> *mbean* blocks specify what data is retrieved from *mbeans* and
> how that data is mapped on the *ncollectd* metrics.
> The block requires one string argument, a name.
> This name is used in the *Connection* blocks (see below) to refer to
> a specific *mbean* block.
> Therefore, the names must be unique.

> The following options are recognized within *mbean* blocks:

> **object-name** *pattern*

> > Sets the pattern which is used to retrieve *MBeans* from the
> > *MBeanServer*.
> > If more than one MBean is returned you should use the **label-from** option
> > (see below) to make the identifiers unique.

> > See also:
> > [http://java.sun.com/javase/6/docs/api/javax/management/ObjectName.html](http://java.sun.com/javase/6/docs/api/javax/management/ObjectName.html)

> **label** *key* *value*

> **label-from** *key* *property*

> > The *object names* used by JMX to identify *MBeans* include so called
> > &#8220;*properties*&#8221; which are basically key-value-pairs.
> > If the given object name is not unique and multiple MBeans are returned,
> > the values of those properties usually differ.

> **metrix-prefix** *prefix*

> **metric** *name*

> > The **metric** blocks map one or more attributes of an *mben*
> > to a value list in *ncollectd*.
> > There must be at least one Value block within each *MBean* block.

> > **label** *key* *value*

> > **label-from** *key* *property*

> > **type** *unknown|gauge|counter*

> > **attribute** *attribute*

> > > Sets the name of the attribute from which to read the value.
> > > You can access the keys of composite types by using a dot to concatenate
> > > the key name to the attribute name.
> > > For example: &#8220;attrib0.key42&#8221;.

**connection**

> Connection blocks specify *how* to connect to an *MBeanServer* and
> what data to retrieve.
> The following configuration options are available:

> **service-url** *url*

> > Specifies how the *MBeanServer* can be reached.
> > Any string accepted by the *JMXServiceURL* is valid.

> > See also:
> > [http://java.sun.com/javase/6/docs/api/javax/management/remote/JMXServiceURL.html](http://java.sun.com/javase/6/docs/api/javax/management/remote/JMXServiceURL.html)

> **label** *key* *value*

> **metrix-prefix** *prefix*

> **collect** *mbean*

> > Configures which of the *MBean* blocks to use with this connection.
> > May be repeated to collect multiple *MBeans* from this server.

> **user** *user*

> > Use *user* to authenticate to the server.
> > If not configured, &#8220;monitorRole&#8221; will be used.

> **password** *password*

> > Use *password* to authenticate to the server.
> > If not given, unauthenticated access is used.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
