NCOLLECTD-TABLE(5) - File Formats Manual

# NAME

**ncollectd-table** - Documentation of ncollectd's table plugin

# SYNOPSIS

	load-plugin table
	plugin table {
	    table /path/to/file {
	        separator  string
	        skip-lines lines
	        metric-prefix prefix
	        label key value
	        interval seconds
	        result {
	            type gauge|unknown|counter|info
	            help help
	            metric name
	            metric-prefix prefix
	            metric-from column
	            label key value
	            label-from column
	            value-from column
	            shift value
	            scale value
	        }
	}

# DESCRIPTION

The **table** plugin provides generic means to parse tabular data and dispatch
user specified values.
Values are selected based on column numbers.

The configuration consists of one or more **table** blocks, each of which
configures one file to parse.
Within each **table** block, there are one or more **result** blocks,
which configure which data to select and how to interpret it.

The following options are available inside a **table** block:

**separator** *string*

> Any character of string is interpreted as a delimiter between the different
> columns of the table.
> A sequence of two or more contiguous delimiters in the table is considered
> to be a single delimiter, i. e. there cannot be any empty columns.
> The default separator is a space.

> A horizontal tab, newline and carriage return may be specified by &#92;t,
> &#92;n and &#92;r respectively.
> Please note that the double backslashes are required because of ncollectd's
> config parsing.

**skip-lines** *lines*

> Skip the number of *lines* at the beginning of the file.

**metric-prefix** *prefix*

> Prepends the *prefix* to the metric name in the block **result**.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.
> Can appear multiple time in the **table** block.

**interval** *seconds*

> The **interval** option allows you to define the length of time between reads.
> If this is not set, the global **interval** will be used.

**result**

> **type** *gauge|unknown|counter|info*

> **help** *help*

> **metric** *name*

> **metric-prefix** *prefix*

> **metric-from** *column*

> **label** *key* *value*

> **label-from** *column*

> > Append the label *key*=*value* to the submitting metrics.

> **value-from** *column*

> **shift** *value*

> > Value is added to gauge metrics values.
> > This value is not applied to counters.

> **scale** *value*

> > The gauge metric value are multiplied by this value.
> > This value is not applied to counters.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
