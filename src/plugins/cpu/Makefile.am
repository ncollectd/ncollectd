pkglib_LTLIBRARIES += cpu.la
cpu_la_SOURCES = src/plugins/cpu/cpu.c
cpu_la_CFLAGS = $(AM_CFLAGS)
cpu_la_LDFLAGS = $(PLUGIN_LDFLAGS)
cpu_la_LIBADD =
if BUILD_WITH_LIBKSTAT
cpu_la_LIBADD += -lkstat
endif
if BUILD_WITH_LIBDEVINFO
cpu_la_LIBADD += -ldevinfo
endif
if BUILD_WITH_LIBSTATGRAB
cpu_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
cpu_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
endif
if BUILD_WITH_PERFSTAT
cpu_la_LIBADD += -lperfstat
endif
