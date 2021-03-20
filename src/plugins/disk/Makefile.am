pkglib_LTLIBRARIES += disk.la
disk_la_SOURCES = src/plugins/disk/disk.c
disk_la_CFLAGS = $(AM_CFLAGS)
disk_la_CPPFLAGS = $(AM_CPPFLAGS)
disk_la_LDFLAGS = $(PLUGIN_LDFLAGS)
disk_la_LIBADD = libignorelist.la
if BUILD_WITH_LIBKSTAT
disk_la_LIBADD += -lkstat
endif
if BUILD_WITH_LIBDEVINFO
disk_la_LIBADD += -ldevinfo
endif
if BUILD_WITH_LIBIOKIT
disk_la_LDFLAGS += -framework IOKit
endif
if BUILD_WITH_LIBSTATGRAB
disk_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
disk_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
endif
if BUILD_WITH_LIBUDEV
disk_la_CPPFLAGS += $(BUILD_WITH_LIBUDEV_CPPFLAGS)
disk_la_LDFLAGS += $(BUILD_WITH_LIBUDEV_LDFLAGS)
disk_la_LIBADD += $(BUILD_WITH_LIBUDEV_LIBS)
endif
if BUILD_FREEBSD
disk_la_LIBADD += -ldevstat -lgeom
endif
if BUILD_WITH_PERFSTAT
disk_la_LIBADD += -lperfstat
endif
