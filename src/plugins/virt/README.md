NCOLLECTD-VIRT(5) - File Formats Manual

# NAME

**ncollectd-virt** - Documentation of ncollectd's virt plugin

# SYNOPSIS

	load-plugin virt
	plugin virt {
	    instance name {
	        connection uri
	        domain [incl|include|excl|exclude] domain
	        block-device [incl|include|excl|exclude] device
	        block-device-format target|source
	        interface-device [incl|include|excl|exclude] device
	        interface-format name|address|number
	        persistent-notification true|false
	        label key value
	        collect flags
	        refresh-interval seconds
	        interval seconds
	        filter {
	            ...
	        }
	    }
	}

# DESCRIPTION

The **virt** plugin allows CPU, disk, network load and other metrics to
be collected for virtualized guests on the machine.
The statistics are collected through libvirt.
Majority of metrics can be gathered without installing any additional
software on guests, especially ncollectd, which runs only on the host system.

In the **instance** block only **connection** is required.
The following options are accepted within each **instance** block:

**connection** *uri*

> Connect to the hypervisor given by *uri*.
> For example if using Xen use:

> >     **connection** "xen:///"

> Details which URIs allowed are given at
> [http://libvirt.org/uri.html](http://libvirt.org/uri.html)

**domain** \[*incl|include|excl|exclude*] *domain*

> Select which domains collected.

**block-device** \[*incl|include|excl|exclude*] *device*

> Select which block devices are collected.

**block-device-format** *target|source*

> If **block-device-format** is set to *target*, the default, then the
> device name seen by the guest will be used for reporting metrics.
> This corresponds to the &lt;target&gt; node in the XML definition of
> he domain.

> If **BlockDeviceFormat** is set to *source*, then metrics will be
> reported using the path of the source, e.g. an image file.
> This corresponds to the &lt;source&gt; node in the XML definition of
> the domain.

**interface-device** \[*incl|include|excl|exclude*] *device*

> Select which interface devices are collected.

**interface-format** *name|address|number*

> When the virt plugin logs interface data, it sets the name of the collected
> data according to this setting.
> The default is to use the path as provided by the hypervisor
> (the "dev" property of the target node), which is equal to setting *name*.

> *address* means use the interface's mac address.
> This is useful since the interface path might change between reboots of a
> guest or across migrations.

> *number* means use the interface's number in guest.

> **Note**: this option determines also what field will be used for
> filtering over interface device (filter value in **interface-device**
> will be applied to name, address or number).
> More info about filtering interfaces can be found in the description of
> **interface-device**.

**persistent-notification** *true|false*

> Override default configuration to only send notifications when there is
> a change in the lifecycle state of a domain.
> When set to true notifications will be sent for every read cycle.
> Default is false.
> Does not affect the stats being dispatched.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**collect** *flags*

> **disk**

> > Report extra statistics like number of flush operations and total
> > service time for read, write and flush operations.
> > Requires libvirt API version *0.9.5* or later.

> **pcpu**

> > Report the physical user/system cpu time consumed by the hypervisor, per-vm.
> > Requires libvirt API version *0.9.11* or later.

> **cpu\_util**

> > Report CPU utilization per domain in percentage.

> **domain\_state**

> > Report domain state and reason.

> **perf**

> > Report performance monitoring events.
> > To collect performance metrics they must be enabled for domain and supported
> > by the platform.
> > Requires libvirt API version *1.3.3* :or later.

> > Note: *perf* metrics can't be collected if *intel\_rdt* plugin
> > is enabled.

> **vcpupin**

> > Report pinning of domain VCPUs to host physical CPUs.

> **disk\_err**

> > Report disk errors if any occured.
> > Requires libvirt API version *0.9.10* or later.

> **fs\_info**

> > Report file system information as a notification.
> > Requires libvirt API version *1.2.11* or later.
> > Can be collected only if *Guest Agent* is installed and configured
> > inside VM.
> > Make sure that installed *Guest Agent* version supports retrieving
> > file system information.

> **disk\_allocation**

> > Report 'disk\_allocation' statistic for disk device.

> > Note: This statistic is only reported for disk devices with 'source' property
> > available.

> **disk\_capacity**

> > Report statistic for disk device.

> > Note: This statistic is only reported for disk devices with 'source' property
> > available.

> **disk\_physical**

> > Report statistic for disk device.

> > Note: This statistic is only reported for disk devices with 'source' property
> > available.

> **memory**

> > Report statistics about memory usage details, provided by libvirt
> > **virDomainMemoryStats**()
> > function.

> **vcpu**

> > Report domain virtual CPUs utilisation.

**refresh-interval** *seconds*

> Refresh the list of domains and devices every *seconds*.
> The default is 60 seconds.
> Setting this to be the same or smaller than the *Interval* will cause the
> list of domains and devices to be refreshed on every iteration.

> Refreshing the devices in particular is quite a costly operation, so if your
> virtualization setup is static you might consider increasing this.
> If this option is set to 0, refreshing is disabled completely.

**interval** *seconds*

> Sets the interval (in seconds) in which the values will be collected from this
> instanced.
> By default the global **interval** setting will be used.

**filter**

> Configure a filter to modify or drop the metrics.
> See **FILTER CONFIGURATION** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
