NCOLLECTD-TURBOSTAT(5) - File Formats Manual

# NAME

**ncollectd-turbostat** - Documentation of ncollectd's turbostat plugin

# SYNOPSIS

	load-plugin turbostat
	plugin turbostat {
	    core-cstates bitmask
	    package-cstates bitmask
	    system-management-interrupt true|false
	    digital-temperature-sensor true|false
	    package-thermal-management true|false
	    running-average-power-limit bitmask
	    tcc-activation-temp temperature
	    restore-affinity-policy all-cpus|restore
	}

# DESCRIPTION

The **turbostat** plugin reads CPU frequency and C-state residency on modern
Intel processors by using *Model Specific Registers*.

**core-cstates** *bitmask*

> Bit mask of the list of core C-states supported by the processor.
> This option should only be used if the automated detection fails.
> Default value extracted from the CPU model and family.

> Currently supported C-states (by this plugin): 3, 6, 7

> All states (3, 6 and 7): (1&lt;&lt;3) + (1&lt;&lt;6) + (1&lt;&lt;7) = 392

**package-cstates** *bitmask*

> Bit mask of the list of packages C-states supported by the processor.
> This option should only be used if the automated detection fails.
> Default value extracted from the CPU model and family.

> Currently supported C-states (by this plugin): 2, 3, 6, 7, 8, 9, 10

> States 2, 3, 6 and 7: (1&lt;&lt;2) + (1&lt;&lt;3) + (1&lt;&lt;6) + (1&lt;&lt;7) = 396

**system-management-interrupt** *true|false*

> Boolean enabling the collection of the I/O System-Management Interrupt counter.
> This option should only be used if the automated detection fails or if you want
> to disable this feature.

**digital-temperature-sensor** *true|false*

> Boolean enabling the collection of the temperature of each core.
> This option should only be used if the automated detection fails or
> if you want to disable this feature.

**package-thermal-management** *true|false*

**running-average-power-limit** *bitmask*

> Bit mask of the list of elements to be thermally monitored.
> This option should only be used if the automated detection fails or if you want
> to disable some collections.
> The different bits of this bit mask accepted by this plugin are:

> **0 ('1')**

> > Package

> **1 ('2'):**

> > DRAM

> **2 ('4'):**

> > Cores

> **3 ('8'):**

> > Embedded graphic device

**tcc-activation-temp** *temperature*

> *Thermal Control Circuit Activation Temperature* of the installed CPU.
> This temperature is used when collecting the temperature of cores or packages.
> This option should only be used if the automated detection fails.
> Default value extracted from MSR\_IA32\_TEMPERATURE\_TARGET.

**restore-affinity-policy** *all-cpus|restore*

> Reading data from CPU has side-effect: collectd process's CPU affinity mask
> changes.
> After reading data is completed, affinity mask needs to be restored.
> This option allows to set restore policy.

> **all-cpus**

> > Restore the affinity by setting affinity to any/all CPUs.
> > The default.

> **Restore**

> > Save affinity using
> > **sched\_getaffinity**()
> > before reading data and restore it after.

> > On some systems,
> > **sched\_getaffinity**()
> > will fail due to inconsistency of the CPU set size between userspace and kernel.
> > In these cases plugin will detect the unsuccessful call and fail with an error,
> > preventing data collection.
> > Most of configurations does not need to save affinity as ncollectd process is
> > allowed to run on any/all available CPUs.

> > If you need to save and restore affinity and get errors like 'Unable to save
> > the CPU affinity', setting 'possible\_cpus' kernel boot option may also help.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

