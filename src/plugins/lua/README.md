NCOLLECTD-LUA(5) - File Formats Manual

# NAME

**ncollectd-lua** - Documentation of ncollectd's lua plugin

# SYNOPSIS

	load-plugin lua
	plugin lua {
	    base-path /path/to/your/lua/scripts
	    load-plugin script.lua
	    plugin name {
	        ...
	    }
	}

# DESCRIPTION

The **lua** plugin embeds a Lua interpreter into collectd and provides an
interface to collectd's plugin system.
This makes it possible to write plugins for collectd in Lua.

The minimum required Lua version is *5.1*.

**base-path** */path/to/your/lua/scripts*

> The directory the **lua** plugin looks in to find script **script**.
> If set, this is also prepended to **package.path**.

**load-plugin** *script.lua*

> The script the **lua** plugin is going to run.
> If **base-path** is not specified, this needs to be an absolute path.

**plugin** *name*

> This block may be used to pass on configuration settings to a Lua script.
> The configuration is converted into a table which is passed to the
> registered configuration callback.

# WRITING YOUR OWN PLUGINS

Writing your own plugins is quite simple, ncollectd manages plugins by means of
**dispatch functions** which call the appropriate **callback functions**
registered by the plugins.
Any plugin basically consists of the implementation of these callback functions
and initializing code which registers the functions with collectd.
See the section **EXAMPLES** below for a really basic example.
The following types of **callback functions** are implemented in the
Lua plugin (all of them are optional):

**read functions**

> These are used to collect the actual data.
> It is called once per interval (see the **interval** configuration option
> of collectd).
> Usually it will call **ncollectd.dispatch\_metric\_family** to dispatch
> the metrics to ncollectd which will pass them on to all registered
> **write functions**.
> If this function does not return 0, interval between its calls will grow
> until function returns 0 again.
> See the **max-read-interval** configuration option of ncollectd.

**write functions**

> These are used to write the dispatched values.
> They are called once for every value that was dispatched by any plugin.

# METATABLES

The following metatables are used to pass values between the Lua plugin
and ncollectd:

**MetricUnknownDouble**

> >     ncollectd.MetricUnknownDouble.new(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> **time**

> **interval**

**MetricUnknownInteger**

> >     ncollectd.MetricUnknownInteger.new(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> **time**

> **interval**

**MetricGaugeDouble**

> >     ncollectd.MetricGaugeDouble.new(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> **time**

> **interval**

**MetricGaugeInteger**

> >     ncollectd.MetricGaugeInteger.new(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> **time**

> **interval**

**MetricCounterDouble**

> >     ncollectd.MetricCounterDouble.new(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> **time**

> **interval**

**MetricCounterInteger**

> >     ncollectd.MetricCounterInteger.new(value\[, labels]\[, time]\[, interval])

> **value**

> **labels**

> **time**

> **interval**

**MetricInfo**

> >     ncollectd.MetricInfo.new(info\[, labels]\[, time]\[, interval])

> **info**

> **labels**

> **time**

> **interval**

**MetricStateSet**

> >     ncollectd.MetricStateSet.new(set\[, labels]\[, time]\[, interval])

> **set**

> **labels**

> **time**

> **interval**

**MetricSummary**

> >     ncollectd.MetricSummary.new(sum, count, quantiles\[, labels]\[, time]\[, interval])

> **sum**

> **count**

> **quantiles**

> **labels**

> **time**

> **interval**

**MetricHistogram**

> >     ncollectd.MetricHistogram.new(sum, buckets\[, labels]\[, time]\[, interval])

> **sum**

> **buckets**

> **labels**

> **time**

> **interval**

**MetricGaugeHistogram**

> >     ncollectd.MetricGaugeHistogram.new(sum, buckets\[, labels]\[, time]\[, interval])

> **sum**

> **buckets**

> **labels**

> **time**

> **interval**

**MetricFamily**

> >     ncollectd.MetricFamily.new(type, name\[, help]\[, unit]\[, metrics])

> dispatch(\[time])

> append(metrics)

> **type**

> **name**

> **help**

> **unit**

> **metrics**

**Notification**

> >     ncollectd.Notification.new(name\[, severity]\[, time]\[, labels]\[, annotations])

> dispatch()

> **type**

> **name**

> **help**

> **unit**

> **metrics**

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

The following functions are provided to Lua modules:

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

> Removes a callback or data-set from collectd's internal list of callback
> functions.
> Every *register\_\*&zwnj;* function has an *unregister\_\*&zwnj;* function.
> *Identifier* is the value that was returned by the register function.

**error, warning, notice, info, debug**(*message*)

> Log a message with the specified severity.

# EXAMPLES

A very simple read function might look like:

	    function read()
	        local file, error = io.open("/proc/loadavg", "r")
	        if (file == nil) then
	            ncollectd.error("Cannot open file: "..error)
	            return -1
	        end
	        local content = file:read "*a"
	        local load = {}
	        local idx = 1
	        for item in string.gmatch(content, "%d*%.?%d+") do
	            load[idx] = item
	            idx = idx + 1
	        end
	        file:close()
	
	        local fam = ncollectd.MetricFamily.new("system_load", ncollectd.METRIC_GAUGE, "System load averages")
	        fam:append(ncollectd.MetricGaugeDouble.new(load[1], {duration = "1m"}))
	        fam:append(ncollectd.MetricGaugeDouble.new(load[2], {duration = "5m"}))
	        fam:append(ncollectd.MetricGaugeDouble.new(load[3], {duration = "15m"}))
	        fam:dispatch()
	
	        return 0
	    end
	

A very simple write function might look like:

	    function write(fam)
	        if (fam.type == ncollectd.METRIC_GAUGE) then
	            io.write("# TYPE "..fam.name.." gauge".."\n")
	            if (fam.help ~= nil) then
	                io.write("# HELP "..fam.name.." "..fam.help.."\n")
	            end
	            local metrics = fam.metrics
	            for i, metric in pairs(metrics) do
	                io.write(fam.name.."{")
	                local labels = metric.labels
	                local j = 0
	                for lkey, lvalue in pairs(labels) do
	                    if j > 0 then
	                        io.write(",")
	                    end
	                    j = j + 1
	                    io.write("\""..lkey.."\"=\""..lvalue.."\"")
	                end
	                io.write("} "..metric.value.." "..math.floor(metric.time*1000).."\n")
	            end
	        end
	
	        return 0
	    end

To register those functions with collectd:

	    ncollectd.register_read(read)     -- pass function as variable
	    ncollectd.register_write("write") -- pass by global-scope function name

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

