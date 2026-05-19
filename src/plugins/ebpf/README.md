NCOLLECTD-EBPF(5) - File Formats Manual

# NAME

**ncollectd-ebpf** - Documentation of ncollectd's ebpf plugin

# SYNOPSIS

	load-plugin ebpf
	plugin ebpf {
	    instance instance-name {
	        script script
	        file file-path
	        label key value
	        interval seconds
	        metric map-name|agreggate-name {
	            name metric-name
	            help help
	            type gauge|counter|histogram|gaugehistogram
	            label key value
	            label-from key index
	        }
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **ebpf** plugin dynamically instruments the running kernel to
aggregate and extract user-defined data.
It compiles an input program in **ply** (See
[https://github.com/wkz/ply](https://github.com/wkz/ply)
) to one or more Linux
bpf(2)
binaries and attaches them to arbitrary points in the kernel using kprobes
and tracepoints.

The configuration of the **ebpf** plugin consists of one or more
**instance** blocks.
Each block requires one string argument as the instance name.
The instance name will be used as *instance* label in the metrics.

The following options are accepted within each **instance** block:

**script** *script*

> String with a **ply** script.

**file** *file-path*

> Path to the file that contains the **ply** script.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from
> this script.

**metric** *map-name|agreggate-name*

> Create a metric from the map or agreggate indicated.

> **name** *metric-name*

> > Set the name of the metric.

> **help** *help*

> > Set the help text of the metric.

> **type** *gauge|counter|histogram|gaugehistogram*

> > Set the metric type. Can be one of *gauge*, *counter*,
> > *histogram* or *gaugehistogram*.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **label-from** *key* *index*

> > Specifies the index of the map or aggregation expresion
> > whose values will be used to create the labels.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5)

# PLY SYNTAX

The syntax is C-like in general, taking its inspiration
dtrace(1)
and, by extension, from
awk(1).

## Probes

A program consists of one or more *probes*, which are analogous to
awk's pattern-action statements.
The syntax for a probe is as follows:

	provider:probe-definition ['/' predicate '/']
	{
	     statement ';'
	    [statement ';' ... ]
	}

The *provider* selects which probe interface to use.
See the PROVIDERS section for more information about each provider.
It is then up to the provider to parse the *probe-definition* to
determine the point(s) of instrumentation.

When tracing, it is often desirable to filter events to match some
criteria.
Because of this, ply allows you to provide a *predicate*,
i.e. an expression that must evaluate to a non-zero value in order for
the probe to be executed.

Then follows a block of *statements* that perform the actual information
gathering.

A provider may define a default probe clause to be used if the user
does not supply one.

## Control of Flow

Probes support basic conditional control of flow via an
*if-statement*, which conforms to the same rules as C's equivalent:

	'if' '(' expr ')'
	    statement ';' | block
	[else
	    statement ';' | block]

In order to ensure that a probe will have a finite run-time the kernel
does not allow backwards branching.
As a result, ply does not have any loop construct like *for* or
*while*.

## Type System

The type system is modeled after C.
As such ply understands the difference between signed and unsigned integers,
the difference between a short and a long long, what separates an integer
from a pointer, how a struct is laid out in memory and so on.
It is not complete though, notably floating point numbers and
unions are missing.

Programs are statically typed, but all types are inferred
automatically.
Thus, the type system is mostly hidden from the user.

Numbers and string literals are specified in the same way as in C.

## Maps

The primary way to extract information is to store it in a *map*,
i.e. in a hash table.
Like
awk(1),
ply dynamically creates any referenced maps and their key and value
types are inferred from the context in which they are used.
All maps are in the global scope and can thus be used both for
extracting data to the end-user, and for carrying data between probes.
Map names follow the rules of identifiers from C.

	mapname[exprs]

Data can be stored in a map by assigning a value to a given key:

	mapname[exprs] = expr

The *delete* keyword can be used to remove an association from a map:

	delete mapname[exprs]

You can also remove all elements in the map using *clear* function.

## Aggregations

More often than not, looking at each individual datum from a trace is
not nearly as helpful as an *aggregation* of the data.
Therefore ply supports aggregating data at the source, thereby
reducing tracing overhead.
Aggregations are syntactically similar to maps, indeed they
are a kind of map, but they are distinguished by a leading '@'.
Also, they can only be assigned the result of one of the following
aggregation functions:

@agg\[exprs] = count()

> Bump a counter.

@agg\[exprs] = sum(scalr-expr)

> Evaluates the argument and aggregates the result.

@agg\[exprs] = quantize(scalar-expr)

> Evaluates the argument and aggregates on the most significant bit
> of the result.
> In other words, it stores the distribution of the expression.

# PROVIDERS

A *provider* makes data available to the user by exporting functions
and variables to the probe.
Function calls use the same syntax as most languages that inherit from C.
In addition to the provider-specific functions, all providers inherits a
set of common functions and variables:

char\[16] comm, char\[16] execname

> *name* of the running process's executable.

u32 cpu

> *CPU ID* of the processor on which the probe fired.

u32 gid

> *Group ID* of the running process.

u32 kpid

> *Kernel PID* of the running process.
> Also known as *pid* by the kernel.
> For a single-threaded process *kpid* is equal to *pid*.
> For multi-threaded processes, *kpid* will be unique while *pid*
> will be the same across all threads.

char\[N] mem(void \*address \[, int size])

> Copy *size* bytes from *address*.
> If *size* is omitted, 64 bytes will be copied.

char\[N] str(void \*address \[, int size])

> Copy a NUL terminated string or *size* bytes from an
> unsafe kernel or user address.
> If the *size* is set should include the terminating NUL byte.
> In case the string length is smaller than size, the target is not
> padded with further NUL bytes.
> If the string length is larger than size, just size-1 bytes are
> copied and the last byte is set to NUL.

s64 time, s64 walltime

> Nanoseconds elaped since system boot.
> *time* is intended for time deltas and *walltime* should be used
> for timestamps.
> They refer to the same data, but with different default output formats.

u32 pid

> *Process ID* of the running process.
> Also known as *thread group ID* (tgid) by the kernel.

void print(...)

> *Print* each expression with its default output format, separated
> by commas and terminated with a newline, to standard out.

void printf(format, ...)

> Prints *formatted output* to standard out.
> In addition to the formats recognized by the printf sitting in your &lt;stdio.h&gt;,
> also recognizes '%v' which will dump the value according to
> the inferred type's default (i.e. how *print* would print it).

int strcmp(char \*a, char \*b)

> Returns -1, 0 or 1 if the first argument is less than, equal to or
> greater than the second argument respectively.
> Strings are compared by their lexicographical order.

u32 uid

> *User ID* of the running process.

## kprobe and kretprobe

These providers use the corresponding kernel features to instrument
arbitrary instructions in the kernel.
The *probe-definition* may be either an address or a symbol name.
When using a symbol name, glob expansion is performed allowing a single
probe to be inserted at multiple locations.
An offset relative to a symbol may also be specfied for kprobes.

Examples:

kretprobe:schedule

> Trace every time schedule returns.

kprobe:SyS\*

> Trace every time a syscall is made.

kprobe:dev\_hard\_start\_xmit+8

> Trace function with offset.

Shared variables:

struct pt\_regs \*regs

> Hardware register contents from when the probe was triggered.
> This matches the definition in &lt;sys/ptrace.h&gt; on your system.

u32 stack

> *Stack trace ID* of the current probe.
> This is just returns an index into a separate map containing the actual
> instruction pointers.
> As a user though, you can think of this function as returning a string
> containing the stack trace at the current location.
> Indeed print(stack) will produce exactly that.

> **CAUTION**: On some architectures (looking at you, ARM), capturing
> stack traces at the entry of a function, before the prologue has
> run, does not work.
> Setting your probe after the prologue will work around the issue
> (typically two instructions, or +8, on ARM).

*kprobe* specific functions:

arg0, arg1 ... argN

> Returns the value of the specified *argument* of the function to
> which the probe was attached, zero-indexed.
> I.e. arg0 is the 1st argument, arg1 is the 2nd, and so on.

> **CAUTION**: ply simply maps registers to arguments according to the
> syscall ABI.
> If your compiler decides to optimize out arguments or do other sneaky things,
> ply will be utterly oblivious to that.

void \*caller

> The program counter, as recorded in regs, at the time the probe
> was triggered.  was attached.
> The default output format will resolve it to a symbolic name
> if one is available.

*kretprobe* specific function:

retval

> Return value of the probed function.

## tracepoint

The tracepoint provider can instrument all stable tracepoints in the kernel.
They are identified by their relative path from the
/sys/kernel/debug/tracing/events directory, where each leaf
directory corresponds to a tracepoint.

Examples:

tracepoint:sched/sched\_wakeup

> Trace every time a process is awoken.

tracepoint:irq/irq\_handler\_entry

> Trace every time an interrupt is handled.

Variables:

struct &lt;X&gt; \*data

> A struct is dynamically generated for each tracepoint by parsing
> its format file.
> I.e. if the contents of the format file looks like the following:

> > name: tcp\_send\_reset
> > ID: 1304
> > format:
> >     field:unsigned short common\_type;    offset:0;    size:2;    signed:0;
> >     field:unsigned char common\_flags;    offset:2;    size:1;    signed:0;
> >     field:unsigned char common\_preempt\_count;    offset:3;    size:1;    signed:0;
> >     field:int common\_pid;    offset:4;    size:4;    signed:1;
> > 
> >     field:const void \* skbaddr;    offset:8;    size:8;    signed:0;
> >     field:const void \* skaddr;    offset:16;    size:8;    signed:0;
> >     field:\_\_u16 sport;    offset:24;    size:2;    signed:0;
> >     field:\_\_u16 dport;    offset:26;    size:2;    signed:0;
> >     field:\_\_u8 saddr\[4];    offset:28;    size:4;    signed:0;
> >     field:\_\_u8 daddr\[4];    offset:32;    size:4;    signed:0;
> >     field:\_\_u8 saddr\_v6\[16];    offset:36;    size:16;    signed:0;
> >     field:\_\_u8 daddr\_v6\[16];    offset:52;    size:16;    signed:0;

> Then data would point to a struct of the following type:

> > struct data {
> >     unsigned short common\_type;
> >     unsigned char common\_flags;
> >     unsigned char common\_preempt\_count;
> >     int common\_pid;
> > 
> >     const void \* skbaddr;
> >     const void \* skaddr;
> >     \_\_u16 sport;
> >     \_\_u16 dport;
> >     \_\_u8 saddr\[4];
> >     \_\_u8 daddr\[4];
> >     \_\_u8 saddr\_v6\[16];
> >     \_\_u8 daddr\_v6\[16];
> > };

Functions:

char\[N] dyn(void \*address \[, int size])

> Copy *size* bytes from a dynamic data pointer in data, i.e. a
> member marked with \_\_data\_loc.
> If *size* is omitted, the default string size determines the number
> of bytes to be copied.

## BEGIN and END

These special providers are called at the beginning and the end of the
tracing session like awk and bpftrace.
The names are case sensitive.
Users can print some messages or fill maps to known info.

## interval

The interval provider will be trigger at each given interval.
Users can specify time and unit (optional).
If unit is omitted, then second is used.
The supported units are:

*	**m**: minutes

*	**s**: seconds (default)

*	**ms**: milli-seconds

*	**us**: micro-seconds

*	**ns**: nano-seconds

Examples:

interval:1

> Called for every second

interval:500ms

> Called for every 500 milli-second

## profile

The profile provider supports profiling by allowing the user to specify
how many times it will fire per-second.
Values of 1-1000 are supported, and the profile provider supports
two probe formats:

profile:\[N]hz

> Profile on all CPUs N times per second

profile:\[C]:\[N]hz

> Profile on CPU C N times per second

# EXAMPLES

## Count number of coredumps

Count the number of coredump that the systemd does.

	plugin ebpf {
	    instance core {
	        script '''
	BEGIN {
	    @core["core"] = sum(0);
	}
	kprobe:do_coredump {
	    @core["core"] = count();
	}'''
	        metric "@core" {
	            name "system_coredump"
	            type counter
	        }
	    }
	}

## Quantize

Record the distribution of the return value of read(2):

	kretprobe:SyS_read
	{
	    @["dist"] = quantize(retval);
	}

## Wildcards

Count all syscalls made on the system, grouped by function:

	kprobe:SyS_*
	{
	    @[caller] = count();
	}

Count all syscalls made by every dd(1) process, grouped by function:

	kprobe:SyS_* / !strcmp(execname, "dd") /
	{
	    @[caller] = count();
	}

## Object Tracking

Record the distribution of the time it takes an *skb* to go from
netif\_receive to *ip\_rcv*:

	kprobe:__netif_receive_skb_core
	{
	    rx[arg0] = time;
	}
	
	kprobe:ip_rcv / rx[arg0] /
	{
	    @["diff"] = quantize(time - rx[arg0]);
	}

# AUTHORS

This man page is based in
ply(1)
man page written by
Tobias Waldekranz &lt;[tobias@waldekranz.com](mailto:tobias@waldekranz.com)&gt;

# COPYRIGHT

Copyright 2018 Tobias Waldekranz

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

