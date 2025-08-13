NCOLLECTD-SOFTIRQ(5) - File Formats Manual

# NAME

**ncollectd-softirq** - Documentation of ncollectd's softirq plugin

# SYNOPSIS

	load-plugin softirq
	plugin softirq {
	    soft-irq [incl|include|excl|exclude] soft-irq
	}

# DESCRIPTION

The **softirq** plugin collect counts of softirq handlers serviced since
boot time, for each CPU.
Reading the file */proc/softirq*.

**soft-irq** \[*incl|include|excl|exclude*] *soft-irq*

> Select this softirq to collectd counters.
> If no configuration if given, the **softirq** plugin will collect data
> from all irqs.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
