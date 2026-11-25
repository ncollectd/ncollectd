NCOLLECTD-SLURM(5) - File Formats Manual

# NAME

**ncollectd-slurm** - Documentation of ncollectd's slurm plugin

# SYNOPSIS

	load-plugin slurm {
	    globals true
	}
	plugin slurm

# DESCRIPTION

The **slurm** plugin collects per-partition SLURM node and job state
information, as well as internal health statistics.
It takes no options.
It should run on a node that is capable of running the *sinfo* and
*squeue* commands, i.e. it has a running slurmd and a valid slurm.conf.
Note that this plugin needs the **globals** option of **load-plugin** set to
**true** in order to function properly, because libslurm loads its own plugins
(e.g. auth/munge) that need to resolve symbols of libslurm.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

