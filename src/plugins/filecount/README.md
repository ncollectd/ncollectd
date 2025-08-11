NCOLLECTD-FILECOUNT(5) - File Formats Manual

# NAME

**ncollectd-filecount** - Documentation of ncollectd's filecount plugin

# SYNOPSIS

	load-plugin filecount
	plugin filecount {
	    directory path {
	        metric-files-size metric
	        help-files-size help
	        metric-files-count metric
	        help-files-count help
	        label key value
	        expr expression
	        name pattern
	        mtime age
	        size size
	        recursive true|false
	        include-hidden true|false
	        regular-only true|false
	    }
	}

# DESCRIPTION

The **filecount** plugin counts the number of files in a certain directory
(and its subdirectories) and their combined size.

The configuration consists of one or more **directory** blocks,
each of which specifies a directory in which to count the files.
Within those blocks, the following options are recognized:

**metric-files-size** *metric*

> Use *metric* as the metric name to dispatch files combined size, the type of
> the metric is **gauge**.

**help-files-size** *help*

> Set the metric help for **metric-files-size**.

**metric-files-count** *metric*

> Use *metric* as the metric name to dispatch number of files, the type of the
> metric is **gauge**.

**help-files-count** *help*

> Set the metric help for **metric-files-count**.

**label** *key* *value*

> Append the label *key*=*value* to the submitting metrics.

**expr** *expression*

> Evaluate expression with the following variables:

> **links**

> > The number of hard links to the file.

> **type**

> > The file is of type:

> > **b**

> > > block (buffered) special

> > **c**

> > > character (unbuffered) special

> > **d**

> > > directory

> > **p**

> > > named pipe (FIFO)

> > **f**

> > > regular file

> > **l**

> > > symbolic  link

> > **s**

> > > socket

> **mode**

> > File's permission  bits.

> **path**

> > Full path file name.

> **name**

> > Base of file name, the path with the leading directories are removed.

> **inode**

> > File's inode number.

> **size**

> > Size in bytes of the file.

> **uid**

> > User Id of the owner of the file.

> **gid**

> > Group Id of the owner of the file.

> **atime**

> > The time of the last access of file data.

> **mtime**

> > The time of last modification of file data.

> **ctime**

> > The file's last status change timestamp (time of last change to the inode).

> **now**

> > Current timestamp.

> **minor**

> > Minor number of the device on which this file resides.

> **major**

> > Major number of the device on which this file resides.

**name** *pattern*

> Only count files that match *pattern*, where *pattern* is a shell-like
> wildcard as understood by
> fnmatch(3).
> Only the **filename** is checked against the pattern, not the entire path.
> In case this makes it easier for you: This option has been named after the
> **-name** parameter to
> find(1).

**mtime** *age*

> Count only files of a specific age: If *age* is greater than zero, only
> files that haven't been touched in the last *age* seconds are counted.
> If *age* is a negative number, this is inversed.
> For example, if **-60** is specified, only files that have been modified
> in the last minute will be counted.

> The number can also be followed by a "multiplier" to easily specify a larger
> timespan.
> When given in this notation, the argument must in quoted, i. e.  must be
> passed as string.
> So the **-60** could also be written as **"-1m"** (one minute).
> Valid multipliers are **s** (second), **m** (minute), **h** (hour),
> **d** (day), **w** (week), and **y** (year).
> There is no "month" multiplier.
> You can also specify fractional numbers, e.g. **"0.5d"** is identical to
> **"12h"**.

**size** *size*

> Count only files of a specific size.
> When *size* is a positive number, only files that are at least this big
> are counted.
> If *size* is a negative number, this is inversed, i. e. only files smaller
> than the absolute value of *size* are counted.

> As with the **mTime** option, a "multiplier" may be added.
> For a detailed description see above.
> Valid multipliers here are **b** (byte), **k** (kilobyte),
> **m** (megabyte), **g** (gigabyte), **t** (terabyte),
> and **p** (petabyte).
> Please note that there are 1000 bytes in a kilobyte, not 1024.

**recursive** *true|false*

> Controls whether or not to recurse into subdirectories.
> Enabled by default.

**include-hidden** *true|false*

> Controls whether or not to include "hidden" files and directories in the count.
> "Hidden" files and directories are those, whose name begins with a dot.
> Defaults to *false*, i.e. by default hidden files and directories
> are ignored.

**regular-only** *true|false*

> Controls whether or not to include only regular files in the count.
> Defaults to *true*, i.e. by default non regular files are ignored.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
