NCOLLECTD-INFO(5) - File Formats Manual

# NAME

**ncollectd-info** - Documentation of ncollectd's info plugin

# SYNOPSIS

	load-plugin info
	plugin info {
	    metric "name" {
	        help "help"
	        label key value
	        info key value
	    }
	}

# DESCRIPTION

The **info** plugin allow to define static info metrics.
By default sends the metric **ncollectd\_info** with the ncollectd version.

The configuration of the **info** plugin consists of one or more **metric**
blocks.
Each block requires one string argument as the metric name.

The following options are accepted within each **metric** mblock:

**help** *help*

> Set the **help** text for the metric.

**label** *key* *value*

> Append the label *key*=*value* to the metric.
> Can appear multiple times.

**info** *key* *value*

> Append the info *key*=*value* to the metric.
> Can appear multiple times.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

