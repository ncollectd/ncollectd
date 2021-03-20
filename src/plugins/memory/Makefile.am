pkglib_LTLIBRARIES += memory.la
memory_la_SOURCES = src/plugins/memory/memory.c
memory_la_CFLAGS = $(AM_CFLAGS)
memory_la_LDFLAGS = $(PLUGIN_LDFLAGS)
memory_la_LIBADD =
if BUILD_WITH_LIBKSTAT
memory_la_LIBADD += -lkstat
endif
if BUILD_WITH_LIBDEVINFO
memory_la_LIBADD += -ldevinfo
endif
if BUILD_WITH_LIBSTATGRAB
memory_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
memory_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
endif
if BUILD_WITH_PERFSTAT
memory_la_LIBADD += -lperfstat
endif
