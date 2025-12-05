NCOLLECTD-MATCH\_TABLE(5) - File Formats Manual

# NAME

**ncollectd-match\_table** - Documentation of ncollectd's table match plugin

# SYNOPSIS

	match table {
	    separator string
	    skip-lines lines
	    metric-prefix prefix
	    label key value
	    metric {
	        type gauge [average|min|max|last|inc|add|persist]|counter [set|add|inc]
	        help help
	        metric name
	        metric-prefix prefix
	        metric-from column
	        label key value
	        label-from key column
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
> The default value is " &#92;t".

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

**metric**

> **type** *gauge* \[*average|min|max|last|inc|add|persist*]|*counter* \[*set|add|inc*]

> > **gauge**

> > > The value is a gauge, you must specify also the type of the gauge:

> > > **average**

> > > > Calculate the average

> > > **min**

> > > > Use the smallest number only.

> > > **max**

> > > > Use the greatest number only.

> > > **last**

> > > > Use the last number found.

> > > **inc**

> > > > Increase the internal counter by one.

> > > **add**

> > > > Add the matched value to the internal counter.

> > > **persist**

> > > > Use the last number found.
> > > > The number is not reset at the end of an interval.
> > > > It is continuously reported until another number is matched.

> > **counter**

> > > The value is a counter, you must specify also the type of the counter:

> > > **set**

> > > > Simply sets the internal counter to this value.

> > > **add**

> > > > Add the matched value to the internal counter.

> > > **inc**

> > > > Increase the internal counter by one.

> **help** *help*

> > Set metric help.

> **metric** *name*

> > Set metric name.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **metric-prefix** *prefix*

> > Prepend the *prefix* to the metrics found in the **metric-from**.

> **metric-from** *column*

> > Configure to read the metric from the column with the zero-based index.

> **label-from** *key* *column*

> > Set the label *key* with the value from the column with the zero-based index.

> **value-from** *column*

> > Configure to read the value from the column with the zero-based index.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

