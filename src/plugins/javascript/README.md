NCOLLECTD-JAVASCRIPT(5) - File Formats Manual

# NAME

**ncollectd-javascript** - Documentation of ncollectd's javascript plugin

# SYNOPSIS

	load-plugin javascript
	plugin javascript {
	    base-path /path/to/your/javascript/scripts
	    load-plugin filename {
	        memory-limit bytes
	        stack-size bytes
	        load-std true|false
	        include filename
	    }
	    plugin name {
	        ...
	    }
	}

# DESCRIPTION

The **javascript** plugin embeds a javascript interpreter into ncollectd
and provides an interface to ncollectd's plugin system.
This makes it possible to write plugins for ncollectd in javascript.

**base-path** */path/to/your/lua/scripts*

> The directory the **javascript** plugin looks in to find scripts.
> If set, this is also prepended to *filename* in **load-plugin**.

**load-plugin** *filename*

> **memory-limit** *bytes*

> > Limit the memory usage of the javascript interpreter.

> **stack-size** *bytes*

> > Limit the stack size of the javscript interpreter.

> **load-std** *true|false*

> > Make 'std' and 'os' modules available to the loaded script.

> **include** *filename*

> > Load file as ES6 module, the file extension must be 'mjs'.

**plugin** *name*

> Configuration for the config callback in the script.

# WRITING YOUR OWN PLUGINS

Writing your own plugins is quite simple, ncollectd manages plugins by means of
**dispatch functions** which call the appropriate **callback functions**
registered by the plugins.
Any plugin basically consists of the implementation of these callback functions
and initializing code which registers the functions with ncollectd.
See the section **EXAMPLES** below for a really basic example.
The following types of **callback functions** are implemented in the
**javascript** plugin (all of them are optional):

**read functions**

> These are used to collect the actual data.
> It is called once per interval (see the **interval** configuration option
> of ncollectd).
> Usually it will call etricFamily.Bdispatch() to dispatch
> the metrics to ncollectd which will pass them on to all registered
> **write functions**.
> If this function does not return 0, interval between its calls will grow
> until function returns 0 again.
> See the **max-read-interval** configuration option of ncollectd.

**write functions**

> These are used to write the dispatched values.
> They are called once for every value that was dispatched by any plugin.

# OBJECTS

The following objects are used to pass values between the javascript plugin
and ncollectd:

**MetricUnknown**

> >     ncollectd.MetricUnknown(value\[, labels]\[, time]\[, interval])

> toString()

> **value**

> **labels**

> **time**

> **interval**

> **type**

> > **FLOAT64**
> > **INT64**

**MetricGauge**

> >     ncollectd.MetricGauge(value\[, labels]\[, time]\[, interval])

> toString()

> **value**

> **labels**

> **time**

> **interval**

> **type**

> > **FLOAT64**
> > **INT64**

**MetricCounter**

> >     ncollectd.MetricCounter(value\[, labels]\[, time]\[, interval])

> toString()

> **value**

> **labels**

> **time**

> **interval**

> **type**

> > **UINT64**
> > **FLOAT64**

**MetricInfo**

> >     ncollectd.MetricInfo(value\[, labels]\[, time]\[, interval])

> toString()

> **value**

> **labels**

> **time**

> **interval**

**MetricStateSet**

> >     ncollectd.MetricStateSet(value\[, labels]\[, time]\[, interval])

> toString()

> **value**

> **labels**

> **time**

> **interval**

**MetricSummary**

> >     ncollectd.MetricSummary(sum, count, quantiles\[, labels]\[, time]\[, interval])

> toString()

> **sum**

> **count**

> **quantiles**

> **labels**

> **time**

> **interval**

**MetricGaugeHistogram**

> >     ncollectd.MetricGaugeHistogram(gsum, buckets\[, labels]\[, time]\[, interval])

> toString()

> **gsum**

> **bucket**

> **labels**

> **time**

> **interval**

**MetricHistogram**

> >     ncollectd.MetricHistogram(sum, bucket\[, labels]\[, time]\[, interval])

> toString()

> **sum**

> **bucket**

> **labels**

> **time**

> **interval**

**MetricFamily**

> >     ncollectd.MetricFamily(type, name\[, help]\[, unit]\[, metrics])

> dispatch(\[time])

> append(metrics)

> toString()

> **type**

> **name**

> **help**

> **unit**

> **metrics**

**Notification**

> >     ncollectd.Notification(name\[, severity]\[, time]\[, labels]\[, annotations])

> dispatch()

> toString()

> **name**

> **severity**

> **time**

> **labels**

> **annotations**

**LOG\_ERR**

**LOG\_WARNING**

**LOG\_NOTICE**

**LOG\_INFO**

**LOG\_DEBUG**

**METRIC\_UNKNOWN**

**METRIC\_GAUGE**

**METRIC\_COUNTER**

**METRIC\_STATE\_SET**

**METRIC\_INFO**

**METRIC\_SUMMARY**

**METRIC\_HISTOGRAM**

**METRIC\_GAUGE\_HISTOGRAM**

**NOTIF\_FAILURE**

**NOTIF\_WARNING**

**NOTIF\_OKAY**

# FUNCTIONS

The following functions are provided to javascript modules:

**register\_\*&zwnj;**(*callback*\[, *data*]\[, *name*]) -&gt; *identifier*

> There are seven different register functions to get callback for seven
> different events.
> With one exception all of them are called as shown above.

> *callback*

> > Is the name of a function, a string with the name of the function or an
> > anonymous function that will be called every time the event is triggered.

> *data*

> > Is an optional data that will be passed back to the callback function
> > every time it is called.
> > If you omit this parameter no data is passed back to your callback.

> *name*

> > Is an optional identifier for this callback.
> > Every callback needs a unique identifier.

> *identifier*

> > Is the full identifier assigned to this callback.

> These functions are called in the various stages of the daemon (see the section
> **WRITING YOUR OWN PLUGINS** above) and are passed the following arguments:

> **register\_config**

> > The only argument passed is a table withe the configuration.
> > Note that you cannot receive the whole config files this way, only **plugin**
> > blocks inside the Lua configuration block.
> > Additionally you will only receive blocks where your callback identifier
> > matches the **name**.

> **register\_init**

> > The callback will be called without arguments.

> **register\_read**(*callback*\[, *interval*&zwnj;*]\[, *&zwnj;*data*&zwnj;*]\[, *&zwnj;*name*&zwnj;*]) -&gt; *&zwnj;*identifier*&zwnj;*&zwnj;*

> > This function takes an additional parameter: *interval*.
> > It specifies the time between calls to the callback function.
> > If this callback function does not return 0 the next call will be delayed by
> > an increasing interval.

> > The callback will be called without arguments.

> **register\_shutdown**

> > The callback will be called without arguments.

> **register\_write**

> > The callback function will be called with one argument passed, which will be a
> > *MetricFamily* metatable.
> > For the layout of *MetricFamily* see above.
> > If this callback function does not return 0 the next call will be delayed by
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

> > The only argument passed is a *Notification* metatable.
> > See above for the layout of this data type.

**unregister\_\*&zwnj;**(*identifier*)

> Removes a callback or data-set from ncollectd's internal list of callback
> functions.
> Every *register\_\*&zwnj;* function has an *unregister\_\*&zwnj;* function.
> *Identifier* is the value that was returned by the register function.

**error, warning, notice, info, debug**(*message*)

> Log a message with the specified severity.

# EXAMPLES

A very simple read function might look like:

	    function read() {
	        const file = std.open("/proc/loadavg", "r");
	        if (!file) {
	            ncollectd.error("Error: fail to open '/proc/loadavg'.");
	            return -1;
	        }
	        const load = file.getline().split(" ");
	        file.close();
	
	        var fam = new ncollectd.MetricFamily("system_load", ncollectd.METRIC_GAUGE, "System load averages");
	        fam.append(new ncollectd.MetricGauge(parseFloat(load[0]), {duration: "1m"}));
	        fam.append(new ncollectd.MetricGauge(parseFloat(load[1]), {duration: "5m"}));
	        fam.append(new ncollectd.MetricGauge(parseFloat(load[2]), {duration: "15m"}));
	        fam.dispatch();
	
	        return 0;
	    }

A very simple write function might look like:

	    function write(fam) {
	        if (fam.type == ncollectd.METRIC_GAUGE) {
	            std.out.puts("# TYPE "+fam.name+" gauge"+"\n");
	            if (fam.help)
	                std.out.puts("# HELP "+fam.name+" "+fam.help+"\n");
	            for (const metric of fam.metrics) {
	                std.out.puts(fam.name+"{");
	                let i = 0;
	                for (const [lkey, lvalue] of Object.entries(metric.labels)) {
	                    if (i > 0)
	                        std.out.puts(",");
	                    i++;
	                    std.out.puts(lkey+"=\""+lvalue+"\"");
	                }
	                std.out.puts("} "+metric.value+" "+Math.floor(metric.time*1000)+"\n");
	            }
	        }
	
	        return 0;
	    }

To register those functions with ncollectd:

	    ncollectd.register_read(read);
	    ncollectd.register_write(write);

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

