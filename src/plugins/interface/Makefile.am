pkglib_LTLIBRARIES += interface.la
interface_la_SOURCES = src/plugins/interface/interface.c
interface_la_CFLAGS = $(AM_CFLAGS)
interface_la_LDFLAGS = $(PLUGIN_LDFLAGS)
interface_la_LIBADD = libignorelist.la
if BUILD_WITH_LIBSTATGRAB
interface_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
interface_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
else
if BUILD_WITH_LIBKSTAT
interface_la_LIBADD += -lkstat
endif
if BUILD_WITH_LIBDEVINFO
interface_la_LIBADD += -ldevinfo
endif
endif
if BUILD_WITH_PERFSTAT
interface_la_LIBADD += -lperfstat
endif
