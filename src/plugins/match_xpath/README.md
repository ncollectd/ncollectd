NCOLLECTD-MATCH\_XPATH(5) - File Formats Manual

# NAME

**ncollectd-match\_xpath** - Documentation of ncollectd's xpath match plugin

# SYNOPSIS

	match xpath {
	    metric-prefix prefix
	    label key value
	    namespace prefixurl
	    metric {
	        xpath xpath-expresion
	        type [gauge [average|min|max|last|inc|add|persist] | counter [set|add|inc]]
	        metric metric
	        help help
	        label key value
	        metric-prefix prefix
	        metric-from xpath-expresion
	        label-from key xpath-expresion
	        value-from xpath-expresion
	        time-from xpath-expresion
	    }
	}

# DESCRIPTION

The **xpath** match plugin parser data as Extensible Markup Language (XML).
The parsed tree is then traversed according to the user's configuration,
using the XML Path Language (XPath).

**metric-prefix** *prefix*

> Prepend the *prefix* to the metrics found in the **metric** blocks.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**namespace** *prefix* *url*

> If an XPath expression references namespaces, they must be specified with
> this option.
> *Prefix* is the "namespace prefix" used in the XML document.
> *URL* is the "namespace name", an URI reference uniquely identifying
> the namespace.
> The option can be repeated to register multiple namespaces.

> Examples:

> >     namespace "s" "http://schemas.xmlsoap.org/soap/envelope/";
> >     namespace "m" "http://www.w3.org/1998/Math/MathML";

**metric**

> **xpath** *xpath-expresion*

> > Specifies how to get one type of information.
> > The string argument must be a valid XPath expression which returns a list
> > of "base elements".

> **type** \[*gauge* \[*average|min|max|last|inc|add|persist*&zwnj;*]* | *counter* \[*set|add|inc*]]

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

> **metric** *metric*

> > Set the metric name.

> **help** *help*

> > Set the  metric help.

> **label** *key* *value*

> > Append the label *key*=*value* to the submitting metrics.

> **metric-prefix** *prefix*

> > The prefix to prepend to the metric name obtained with **metric-from**.

> **metric-from** *xpath-expresion*

> > Specifies a XPath expression to use for the metric name.

> **label-from** *key* *xpath-expresion*

> > Specifies a XPath expression to use for value of the label *key*.

> **value-from** *xpath-expresion*

> > Specifies a XPath expression to use for reading the value.
> > This option is required.

> **time-from** *xpath-expresion*

> > Specifies a XPath expression to use for determining the
> > timestamp of the metric.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
