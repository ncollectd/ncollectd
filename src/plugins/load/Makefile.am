pkglib_LTLIBRARIES += load.la
load_la_SOURCES = src/plugins/load/load.c
load_la_CFLAGS = $(AM_CFLAGS)
load_la_LDFLAGS = $(PLUGIN_LDFLAGS)
load_la_LIBADD =
if BUILD_WITH_LIBSTATGRAB
load_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
load_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
endif
if BUILD_WITH_PERFSTAT
load_la_LIBADD += -lperfstat
endif
