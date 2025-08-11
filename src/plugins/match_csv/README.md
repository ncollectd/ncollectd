NCOLLECTD-MATCH\_CSV(5) - File Formats Manual

# NAME

**ncollectd-match\_csv** - Documentation of ncollectd's csv match plugin

# SYNOPSIS

	match csv {
	    metric-prefix prefix
	    label key value
	    time-from index
	    field-separator char
	    metric {
	        metric  metric
	        help help
	        type gauge [average|min|max|last|inc|add|persist]|counter [set|add|inc]
	        label key value
	        metric-prefix prefix
	        metric-from index
	        label-from key index
	        value-from index
	    }
	}

# DESCRIPTION

The **match\_csv** plugin parse lines in the CSV format,

**metric-prefix** *prefix*

> Prepend the *prefix* to the metrics found in the **metric** blocks.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**time-from** *index*

> Configure to read the timestamp of the metric from the field with the
> zero-based index.

**field-separator** *char*

> Any character a delimiter between the different columns of the table.

**metric**

> **metric** *metric*

> > Set metric name.

> **help** *help*

> > Set metric help.

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
> > > > It is continously reported until another number is matched.

> > **counter**

> > > The value is a counter, you must specify also the type of the counter:

> > > **set**

> > > > Simply sets the internal counter to this value.

> > > **add**

> > > > Add the matched value to the internal counter.

> > > **inc**

> > > > Increase the internal counter by one.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **metric-prefix** *prefix*

> > Prepend the *prefix* to the metrics found in the **metric-from**.

> **metric-from** *index*

> > Configure to read the metric from the field with the zero-based index.

> **label-from** *key* *index*

> > Set the label *key* with the value from the field with the zero-based index.

> **value-from** *index*

> > Configure to read the value from the field with the zero-based index.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
