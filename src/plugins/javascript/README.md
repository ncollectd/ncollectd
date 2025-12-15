NCOLLECTD-JAVASCRIPT(5) - File Formats Manual

# NAME

**ncollectd-javascript** - Documentation of ncollectd's javascript plugin

# SYNOPSIS

	load-plugin javascript
	plugin javascript {
	    instance instance {
	        memory-limit bytes
	        stack-size bytes
	        interval seconds
	        load-std true|false
	        include filename
	        script filename
	        config {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **javascript** plugin embeds a javascript interpreter into ncollectd
and provides an interface to ncollectd's plugin system.
This makes it possible to write plugins for ncollectd in javascript.

Each instance is a javascript interpreter, the following options are accepted
within each **instance** block:

**memory-limit** *bytes*

> Limit the memory usage of the javascript interpreter.

**stack-size** *bytes*

> Limit the stack size of the javscript interpreter.

**interval** *seconds*

> Sets the interval (in seconds) in which read callbacks will be called.

**load-std** *true|false*

> Make 'std' and 'os' modules available to the loaded script.

**include** *filename*

> Load file as ES6 module, the file extension must be 'mjs'.

**script** *filename*

> Load file as ES6 script.

**config**

> Configuration for the config callback in the script.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

