NCOLLECTD-APACHE(5) - File Formats Manual

# NAME

**ncollectd-apache** - Documentation of ncollectd's apache plugin

# SYNOPSIS

	load-plugin info
	plugin info {
	    metric "name" {
	        help "help"
	        type gauge|counter|unknown|info
	        label key value
	        value value
	    }
	}

# DESCRIPTION

The **info** plugin allow to define static metrics.
By default sends the metric **ncollectd\_info** with the ncollectd version.

The configuration of the **info** plugin consists of one or more **metric**
blocks.
Each block requires one string argument as the metric name.

The following options are accepted within each **metric** mblock:

**help** *help*

> Set the **help** text for the metric.

**type** *gauge|counter|unknown|info*

> Defines the metric type, must be *gauge*, *counter*, *unknown* or
> *info*.
> If not set is *Info*.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple times.

**value** *value*

> Set the value of the metric.
> If not set is 1.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
