NCOLLECTD-MATCH\_REGEX(5) - File Formats Manual

# NAME

**ncollectd-match\_regex** - Documentation of ncollectd's regex match plugin

# SYNOPSIS

	match regex {
	    metric-prefix prefix
	    label key value
	    metric {
	        regex regex
	        exclude-regex regexp
	        type [gauge [average|min|max|last|inc|add|persist] | counter [set|add|inc]]
	        metric metric
	        help help
	        label key index
	        metric-prefix prefix
	        metric-from index
	        label-from key index
	        value-from index
	        time-from index
	    }
	}

# DESCRIPTION

The **match\_regex** plugin parses data matching regular expressions.

**metric-prefix** *prefix*

**label** *key* *value*

**metric**

> **regex** *regex*

> > Sets the regular expression to use for matching against a line.

> **exclude-regex** *regexp*

> > Sets an optional regular expression to use for excluding lines from the match.

> **type** \[*gauge* \[*average|min|max|last|inc|add|persist*] | *counter* \[*set|add|inc*]]

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

> **metric** *metric*

> **help** *help*

> **label** *key* *index*

> **metric-prefix** *prefix*

> **metric-from** *index*

> **label-from** *key* *index*

> **value-from** *index*

> **time-from** *index*

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
