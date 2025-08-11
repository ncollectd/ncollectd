NCOLLECTD-LUA(5) - File Formats Manual

# NAME

**ncollectd-lua** - Documentation of ncollectd's lua plugin

# SYNOPSIS

	load-plugin lua
	plugin lua {
	    base-path /path/to/your/lua/scripts
	    script script.lua
	}

# DESCRIPTION

The **lua** plugin embeds a Lua interpreter into collectd and provides an
interface to collectd's plugin system.
This makes it possible to write plugins for collectd in Lua.

The minimum required Lua version is *5.1*.

**base-path** */path/to/your/lua/scripts*

> The directory the **lua** plugin looks in to find script **script**.
> If set, this is also prepended to **package.path**.

**script** *script.lua*

> The script the **lua** plugin is going to run.
> If **base-path** is not specified, this needs to be an absolute path.

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

# FUNCTIONS

The following functions are provided to Lua modules:

**register\_read**(*callback*)

> Function to register read callbacks.
> The callback will be called without arguments.
> If this callback function does not return 0 the next call will be delayed by
> an increasing interval.

**register\_write**(*callback*)

> Function to register write callbacks.
> The callback function will be called with one argument passed, which will be a
> table of values.
> If this callback function does not return 0 next call will be delayed by
> an increasing interval.

**log\_error, log\_warning, log\_notice, log\_info, log\_debug**(*message*)

> Log a message with the specified severity.

# EXAMPLES

A very simple read function might look like:

	  function read()
	    ncollectd.log_info("read function called")
	    t = {
	        host = 'localhost',
	        plugin = 'myplugin',
	        type = 'counter',
	        values = {42},
	    }
	    ncollectd.dispatch_values(t)
	    return 0
	  end

A very simple write function might look like:

	  function write(vl)
	    for i = 1, #vl.values do
	      ncollectd.log_info(vl.host .. '.' .. vl.plugin .. '.' .. vl.type .. ' ' .. vl.values[i])
	    end
	    return 0
	  end

To register those functions with collectd:

	  ncollectd.register_read(read)     -- pass function as variable
	  ncollectd.register_write("write") -- pass by global-scope function name

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
