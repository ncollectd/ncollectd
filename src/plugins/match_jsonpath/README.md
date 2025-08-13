NCOLLECTD-MATCH\_JSONPATH(5) - File Formats Manual

# NAME

**ncollectd-match\_jsonpath** - Documentation of ncollectd's jsonpath match plugin

# SYNOPSIS

	match jsonpath {
	    metric-prefix prefix
	    label key value
	    metric {
	        path path
	        metric-prefix prefix
	        metric metric
	        metric-path path
	        metric-root-path path
	        type gauge [average|min|max|last|inc|add|persist]|counter [set|add|inc]
	        help help
	        help-path path
	        help-root-path path
	        label key value
	        label-path key path
	        label-root-path key path
	        value-path path
	        value-root-path path
	        time-path path
	        time-root-path path
	    }
	}

# DESCRIPTION

The **match\_jsonpath** plugin parse text in json format using jsonpath
selectors.

**metric-prefix** *prefix*

> Prepend the *prefix* to the metrics found in the **metric** blocks.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**metric**

> **path** *path*

> > Use the jsonpath selector *path* to extract metrics.

> **metric-prefix** *prefix*

> > Prepend the *prefix* to the metrics found.

> **metric** *metric*

> > Set the metric name.

> **metric-path** *path*

> > Set metric value from the jsonpath selector *path* in the current node.

> **metric-root-path** *path*

> > Set metric value from the jsonpath selector *path* in the root node.

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

> **help-path** *path*

> > Set metric help from the jsonpath selector *path* in the current node.

> **help-root-path** *path*

> > Set metric help from the jsonpath selector *path* in the root node.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **label-path** *key* *path*

> > Set the label *key* with the value from the jsonpath selector *path*
> > in the current node.

> **label-root-path** *key* *path*

> > Set the label *key* with the value from the jsonpath selector *path*
> > in the root node.

> **value-path** *path*

> > Set metric value from the jsonpath selector *path* in the current node.

> **value-root-path** *path*

> > Set metric value from the jsonpath selector *path* in the root node.

> **time-path** *path*

> > Set metric timestamp in seconds from the jsonpath selector *path*
> > in the current node.

> **time-root-path** *path*

> > Set metric timestamp in seconds from the jsonpath selector *path*
> > in the root node.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
