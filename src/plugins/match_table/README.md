NCOLLECTD-MATCH\_TABLE(5) - File Formats Manual

# NAME

**ncollectd-match\_table** - Documentation of ncollectd's table match plugin

# SYNOPSIS

	match table {
	    separator  string
	    skip-lines lines
	    metric-prefix prefix
	    label key value
	    metric {
	        type gauge|unknow|counter|info
	        help help
	        metric name
	        metric-prefix prefix
	        metric-from column
	        label key value
	        label-from column
	        value-from column
	    }
	}

# DESCRIPTION

The **match\_table** plugin provides generic means to parse tabular data
and dispatch user specified values.
Values are selected based on column numbers.

The configuration consists of one or more **table** blocks, each of which
configures one file to parse.
Within each **table** block, there are one or more **result** blocks,
which configure which data to select and how to interpret it.

The following options are available inside a **match** block:

**separator** *string*

> Any character of string is interpreted as a delimiter between the different
> columns of the table.
> A sequence of two or more contiguous delimiters in the table is considered
> to be a single delimiter, i. e. there cannot be any empty columns.
> This option is mandatory.

> A horizontal tab, newline and carriage return may be specified by &#92;t,
> &#92;n and &#92;r respectively.
> Please note that the double backslashes are required because of collectd's
> config parsing.

**skip-lines** *lines*

> Skip the number of *lines* at the beginning of the file.

**metric-prefix** *prefix*

> Prepends the *prefix* to the metric name in the block **result**.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **table** block.

**result**

> **type** *gauge|unknow|counter|info*

> **help** *help*

> **metric** *name*

> **metric-prefix** *prefix*

> **metric-from** *column*

> **label** *key* *value*

> **label-from** *column*

> > Append the label *key*=*value* to the submitting metrics.

> **value-from** *column*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
