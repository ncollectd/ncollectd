NCOLLECTD-IRQ(5) - File Formats Manual

# NAME

**ncollectd-irq** - Documentation of ncollectd's irq plugin

# SYNOPSIS

	load-plugin irq
	plugin irq {
	    irq [incl|include|excl|exclude] irq
	}

# DESCRIPTION

The **irq** plugin collects the number of times each interrupt
has been handled by the operating system.

The plugin has the following options:

**irq** \[*incl|include|excl|exclude*] *irq*

> Select this *irq*.
> By default these irqs will then be collected.
> If no configuration if given, the **irq** plugin will collect data from all
> irqs.
> This may not be practical, especially if no interrupts happen.
> Thus, you can use the **irq** option to pick the interrupt you're
> interested in.
> See **INCLUDE AND EXCLUDE LISTS** in
> ncollectd.conf(5).

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
