NCOLLECTD-GPU\_NVIDIA(5) - File Formats Manual

# NAME

**ncollectd-gpu\_nvidia** - Documentation of ncollectd's gpu\_nvidia plugin

# SYNOPSIS

	load-plugin gpu_nvidia
	plugin gpu_nvidia {
	    gpu-index index
	    ignore-gpu-selected true|false
	}

# DESCRIPTION

The **gpu\_nvidia** plugin efficiently collects various statistics from the
system's NVIDIA GPUs using the NVML library.
Currently collected are fan speed, core temperature, percent load, percent
memory used, compute and memory frequencies, and power consumption.

**gpu-index** *index*

> If one or more of these options is specified, only GPUs at that index (as
> determined by nvidia-utils through *nvidia-smi*) have statistics collected.
> If no instance of this option is specified, all GPUs are monitored.

**ignore-gpu-selected** *true|false*

> If set to **true**, all detected GPUs **except** the ones at indices
> specified by **gpu-index** entries are collected.
> For greater clarity, setting **ignore-gpu-selected** without any
> **gpu-index** directives will result in **no** statistics being collected.

# SEE ALSO

ncollectd(1),
ncollectd.conf(5)

ncollectd - - -
