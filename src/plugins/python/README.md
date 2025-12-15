NCOLLECTD-PYTHON(5) - File Formats Manual

# NAME

**ncollectd-python** - Documentation of ncollectd's python plugin

# SYNOPSIS

	load-plugin plugin
	plugin plugin {
	    encoding encoding
	    module-path /path/to/your/python/modules
	    log-traces true|false
	    interactive true|false
	    import "spam"
	    module spam  {
	      spam "wonderful" "lovely"
	    }
	}

# DESCRIPTION

The **python** plugin embeds a Python-interpreter into ncollectd and provides
an interface to ncollectd's plugin system.
This makes it possible to write plugins for ncollectd in Python.

The minimum required Python version is *2.6*.

Available configuration options:

**encoding** *encoding*

> The default encoding for Unicode objects you pass to ncollectd.
> If you omit this option it will default to **ascii** on *Python 2*.
> On *Python 3* it will always be **utf-8**, as this function was removed,
> so this will be silently ignored.

> These defaults are hardcoded in Python and will ignore everything else,
> including your locale.

**module-path** */path/to/your/python/modules*

> Prepends */path/to/your/python/modules* to sys.path.
> You won't be able to import any scripts you wrote unless they are located in
> one of the directories in this list.
> Please note that it only has effect on plugins loaded after this option.
> You can use multiple **module-path** lines to add more than one directory.

**log-traces** *true|false*

> If a Python script throws an exception it will be logged by ncollectd with the
> name of the exception and the message.
> If you set this option to true it will also log the full stacktrace just like
> the default output of an interactive Python interpreter.
> This does not apply to the CollectError exception, which will never log a
> stacktrace.

> This should probably be set to false most of the time but is very useful for
> development and debugging of new modules.

**interactive** *true|false*

> This option will cause the module to launch an interactive Python interpreter
> that reads from and writes to the terminal.
> Note that ncollectd will terminate right after starting up if you try to run
> it as a daemon while this option is enabled so make sure to start ncollectd
> with the -f option.

> The **ncollectd** module is **not** imported into the interpreter's
> globals.
> You have to do it manually.
> Be sure to read the help text of the module, it can be used as a reference
> guide during coding.

> This interactive session will behave slightly differently from a daemonized
> ncollectd script as well as from a normal Python interpreter:

> **1.**

> > ncollectd will try to import the readline module to give you a decent
> > way of entering your commands.
> > The daemonized ncollectd won't do that.

> **2.**

> > Python will be handling **SIGINT**.
> > Pressing Ctrl+C will usually cause ncollectd to shut down.
> > This would be problematic in an interactive session, therefore Python will be
> > handling it in interactive sessions.
> > This allows you to use Ctrl+C to interrupt Python code without killing
> > ncollectd.
> > This also means you can catch KeyboardInterrupt exceptions which does
> > not work during normal operation.

> > To quit ncollectd send **EOF** (press Ctrl+D at the beginning
> > of a new line).

**import** name

> Imports the python script *name* and loads it into the ncollectd
> python process.
> If your python script is not found, be sure its directory exists in
> python's sys.path.
> You can prepend to the sys.path using the **module-path**
> configuration option.

**module** name

> This block may be used to pass on configuration settings to a Python module.
> The configuration is converted into an instance of the **Config** class
> which is passed to the registered configuration callback.
> See below for details about the **Config** class and how to register
> callbacks.

> The *name* identifies the callback.

# STRINGS

There are a lot of places where strings are sent from ncollectd to Python and
from Python to ncollectd.
How exactly this works depends on whether byte or unicode strings or Python2
or Python3 are used.

Python2 has *str*, which is just bytes, and *unicode*.
Python3 has *str*, which is a unicode object, and *bytes*.

When passing strings from Python to ncollectd all of these object are supported
in all places, however *str* should be used if possible.
These strings must not contain a NUL byte.
Ignoring this will result in a *TypeError* exception.
If a byte string was used it will be used as is by ncollectd.
If a unicode object was used it will be encoded using the default encoding
(see above).
If this is not possible Python will raise a *UnicodeEncodeError* exception.

When passing strings from ncollectd to Python the behavior depends on the
Python version used.
Python2 will always receive a *str* object.
Python3 will usually receive a *str* object as well, however the original
string will be decoded to unicode using the default encoding.
If this fails because the string is not a valid sequence for this encoding a
*bytes* object will be returned instead.

# WRITING YOUR OWN PLUGINS

Writing your own plugins is quite simple. ncollectd manages plugins by means of
**dispatch functions** which call the appropriate **callback functions**
registered by the plugins.
Any plugin basically consists of the implementation of these callback functions
and initializing code which registers the functions with ncollectd.
See the section **EXAMPLES** below for a really basic example.
The following types of **callback functions** are known to ncollectd
(all of them are optional):

**configuration functions**

> These are called during configuration if an appropriate
> **module** block has been encountered.
> It is called once for each **module** block which matches the name of
> the callback as provided with the register\_config method - see below.

> Python thread support has not been initialized at this point so do not use any
> threading functions here!

**init functions**

> These are called once after loading the module and before any
> calls to the read and write functions.
> It should be used to initialize the internal state of the plugin
> (e. g. open sockets, ...).
> This is the earliest point where you may use threads.

**read functions**

> These are used to collect the actual data.
> It is called once per interval (see the **interval** configuration
> option of ncollectd).
> Usually it will call plugin\_dispatch\_metric\_family to dispatch the
> metrics to ncollectd which will pass them on to all registered
> **write functions**.
> If this function throws any kind of exception the plugin will be skipped for an
> increasing amount of time until it returns normally again.

**write functions**

> These are used to write the dispatched values.
> It is called once for every value that was dispatched by any plugin.

**log functions**

> These are used to pass messages of plugins or the daemon itself to the user.

**notification function**

> These are used to act upon notifications.
> In general, a notification is a status message that may be associated with
> a data instance.

**shutdown functions**

> These are called once before the daemon shuts down.
> It should be used to clean up the plugin (e.g. close sockets, ...).

Any function (except log functions) may throw an exception in case of
errors.
The exception will be passed on to the user using ncollectd's logging mechanism.
If a log callback throws an exception it will be printed to standard
error instead.

See the documentation of the various **register\_** methods in the section
**FUNCTIONS** below for the number and types of arguments passed to each
**callback function**.
This section also explains how to register **callback functions**
with ncollectd.

To enable a module, copy it to a place where Python can find it (i. e. a
directory listed in sys.path) just as any other Python plugin and add
an appropriate **import** option to the configuration file.
After restarting ncollectd you're done.

# CLASSES

The following complex types are used to pass values between the Python plugin
and ncollectd:

**NCollectdError**

> This is an exception.
> If any Python script raises this exception it will still be treated like an
> error by ncollectd but it will be logged as a warning instead of an error
> and it will never generate a stacktrace.

> >     class NCollectdError(Exception)

> Basic exception for ncollectd Python scripts.
> Throwing this exception will not cause a stacktrace to be logged, even if
> LogTraces is enabled in the config.

**Config**

> The Config class is an object which keeps the information provided in the
> configuration file.
> The sequence of children keeps one entry for each configuration option.
> Each such entry is another Config instance, which may nest further if
> nested blocks are used.

> >     class Config(object)

> This represents a piece of ncollectd's config file.
> It is passed to scripts with config callbacks (see register\_config)
> and is of little use if created somewhere else.

> It has no methods beyond the bare minimum and only exists for its data members.

> Data descriptors defined here:

> **key**

> > This is the keyword of this item, i.e. the first word of any given line in the
> > config file.
> > It will always be a string.

> **values**

> > This is a tuple (which might be empty) of all value, i.e. words following the
> > keyword in any given line in the config file.

> > Every item in this tuple will be either a string, a float or a boolean,
> > depending on the contents of the configuration file.

> **children**

> > This is a tuple of child nodes.
> > For most nodes this will be empty.
> > If this node represents a block instead of a single line of the config file
> > it will contain all nodes in this block.

**Metric**

> >     class Metric(\[labels]\[, time]\[, interval])

> **labels**

> > It has to be a dictionary of numbers, strings or bools.
> > All keys must be strings.

> **time**

> > This is the Unix timestamp of the time this metric was read.
> > For dispatching metrics this can be set to zero which means "now".

> **interval**

> > The interval is the timespan in seconds between two submits for the same data
> > source.
> > This value has to be a positive integer, so you can't submit more than
> > one value per second.
> > If this member is set to a non-positive value, the default value as specified
> > in the config file will be used (default: 10).

> > If you submit values more often than the specified interval, the average will
> > be used.
> > If you submit less values, your graphs will have gaps.

**MetricUnknownDouble**

> >     class MetricUnknownDouble(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricUnknownLong**

> >     class MetricUnknownLong(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricGaugeDouble**

> >     class MetricGaugeDouble(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricGaugeLong**

> >     class MetricGaugeLong(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricCounterDouble**

> >     class MetricCounterDouble(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricCounterULong**

> >     class MetricCounterUlong(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricInfo**

> >     class MetricInfo(info\[, labels]\[, time]\[, interval])

> **info**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricStateSet**

> >     class MetricStateSet(set\[, labels]\[, time]\[, interval])

> **set**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricSummary**

> >     class MetricSummary(sum, count, quantiles\[, labels]\[, time]\[, interval])

> **sum**

> **count**

> **quantiles**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricHistorgram**

> >     class MetricHistorgram(sum, buckets\[, labels]\[, time]\[, interval])

> **sum**

> **buckets**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricGaugeHistorgram**

> >     class MetricGaugeHistorgram(sum, buckets\[, labels]\[, time]\[, interval])

> **sum**

> **buckets**

> **labels**

> > See **class Metric**.

> **time**

> > See **class Metric**.

> **interval**

> > See **class Metric**.

**MetricFamily**

> >     class MetricFamily(type, name\[, help]\[, unit]\[, metrics])

> Methods defined here:

> >     dispatch(\[metrics]\[, time]) -&gt; None.
> >     append(metrics) -&gt; None.

> Dispatch this instance to the ncollectd process.
> The object has members for each of the possible arguments for this method.
> For a detailed explanation of these parameters see the member of the same same.

> If you do not submit a parameter the value saved in its member will be
> submitted.
> If you do provide a parameter it will be used instead,
> without altering the member.

> Data descriptors defined here:

> **type**

> > The type of this metric family.
> > Assign or compare to **METRIC\_TYPE\_UNKNOWN**, **METRIC\_TYPE\_GAUGE**,
> > **METRIC\_TYPE\_COUNTER**, **METRIC\_TYPE\_STATE\_SET**,
> > **METRIC\_TYPE\_INFO**, **METRIC\_TYPE\_SUMMARY**, **METRIC\_TYPE\_HISTOGRAM**
> > or **METRIC\_TYPE\_GAUGE\_HISTOGRAM**.

> **name**

> **help**

> **unit**

> **metrics**

**Notification**

> A notification is an object defining the severity and message of the status
> message as well as an identification of a data instance.

> >     class Notification(name\[, severity]\[, time]\[, labels]\[, annotations])

> The Notification class is a wrapper around the ncollectd notification.
> It can be used to notify other plugins about bad stuff happening.
> It works similar to Values but has a severity and a message instead of interval
> and time.
> Notifications can be dispatched at any time and can be received with
> register\_notification.

> Methods defined here:

> >     dispatch(\[name]\[, severity]\[, time]\[, labels]\[, annotations]) -&gt; None.

> Dispatch this instance to the ncollectd process.
> The object has members for each of the possible arguments for this method.
> For a detailed explanation of these parameters see the member of the same same.

> If you do not submit a parameter the value saved in its member will be
> submitted.
> If you do provide a parameter it will be used instead, without altering
> the member.

> Data descriptors defined here:

> **name**

> > Name of the notification.

> **severity**

> > The severity of this notification.
> > Assign or compare to **NOTIF\_FAILURE**, **NOTIF\_WARNING** or
> > **NOTIF\_OKAY**.

> **time**

> **label**

> **annotations**

# FUNCTIONS

The following functions provide the C-interface to Python-modules.

**register\_\*&zwnj;**(*callback*\[, *data*]\[, *name*]) -&gt; *identifier*

> There are seven different register functions to get callback for seven
> different events.
> With one exception all of them are called as shown above.

> *callback*

> > Is a callable object that will be called every time the event is
> > triggered.

> *data*

> > Is an optional object that will be passed back to the callback function
> > every time it is called.
> > If you omit this parameter no object is passed back to your callback,
> > not even None.

> *name*

> > Is an optional identifier for this callback.
> > The default name is **python**.*module*.
> > *module* is taken from the **\_\_module\_\_** attribute of your
> > callback function.
> > Every callback needs a unique identifier, so if you want to register the same
> > callback multiple times in the same module you need to
> > specify a name here.
> > Otherwise it's safe to ignore this parameter.

> *identifier*

> > Is the full identifier assigned to this callback.

> These functions are called in the various stages of the daemon (see the section
> **WRITING YOUR OWN PLUGINS** above) and are passed the following arguments:

> **register\_config**

> > The only argument passed is a *Config* object.
> > See above for the layout of this data type.
> > Note that you cannot receive the whole config files this way, only **module**
> > blocks inside the Python configuration block.
> > Additionally you will only receive blocks where your callback identifier
> > matches **python**.*blockname*.

> **register\_init**

> > The callback will be called without arguments.

> **register\_read**(*callback*\[, *interval*&zwnj;*]\[, *&zwnj;*data*&zwnj;*]\[, *&zwnj;*name*&zwnj;*]) -&gt; *&zwnj;*identifier*&zwnj;*&zwnj;*

> > This function takes an additional parameter: *interval*.
> > It specifies the time between calls to the callback function.

> > The callback will be called without arguments.

> **register\_shutdown**

> > The callback will be called without arguments.

> **register\_write**

> > The callback function will be called with one argument passed, which will be a
> > *MetricFamily* object.
> > For the layout of *MetricFamily* see above.
> > If this callback function throws an exception the next call will be delayed by
> > an increasing interval.

> **register\_log**

> > The arguments are *severity* and *message*.
> > The severity is an integer and small for important messages and high for less
> > important messages.
> > The least important level is **LOG\_DEBUG**, the most important level
> > is **LOG\_ERR**.
> > In between there are (from least to most important): **LOG\_INFO**,
> > **LOG\_NOTICE**, and **LOG\_WARNING**.
> > *message* is simply a string **without** a newline at the end.

> > If this callback throws an exception it will **not** be logged.
> > It will just be printed to **sys.stderr** which usually means
> > silently ignored.

> **register\_notification**

> > The only argument passed is a *Notification* object.
> > See above for the layout of this data type.

**unregister\_\*&zwnj;**(*identifier*) -&gt; None

> Removes a callback or data-set from collectd's internal list of callback
> functions.
> Every *register\_\*&zwnj;* function has an *unregister\_\*&zwnj;* function.
> *identifier* is either the string that was returned by the register function
> or a callback function.
> The identifier will be constructed in the same way as for the register
> functions.

**error**, **warning**, **notice**, **info**, **debug**(*message*)

> Log a message with the specified severity.

# EXAMPLES

Any Python module will start similar to:

	    import ncollectd

A very simple read function might look like:

	    import random
	
	    def read(data=None):
	        fam = ncollectd.MetricFamily(ncollectd.METRIC_TYPE_GAUGE, 'python.spam')
	        fam.dispatch([ncollectd.MetricGaugeDouble(random.random() * 100)])

A very simple write function might look like:

	    def write(fam, data=None):
	        for i in vl.values:
	            print "%s (%s): %f" % (vl.plugin, vl.type, i)

To register those functions with collectd:

	    ncollectd.register_read(read)
	    ncollectd.register_write(write)

See the section **CLASSES** above for a complete documentation of the data
types used by the read, write and match functions.

# CAVEATS

ncollectd is heavily multi-threaded.
Each ncollectd thread accessing the Python plugin will be mapped to a Python
interpreter thread.
Any such thread will be created and destroyed transparently and on-the-fly.

Hence, any plugin has to be thread-safe if it provides several entry points
from ncollectd (i. e. if it registers more than one callback or if a
registered callback may be called more than once in parallel).

The Python thread module is initialized just before calling the init callbacks.
This means you must not use Python's threading module prior to this point.
This includes all config and possibly other callback as well.

The python plugin exports the internal API of ncollectd which is considered
unstable and subject to change at any time.
We try hard to not break backwards compatibility in the Python API during
the life cycle of one major release.
However, this cannot be guaranteed at all times.
Watch out for warnings dispatched by the python plugin after upgrades.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

