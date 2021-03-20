pkglib_LTLIBRARIES += uptime.la
uptime_la_SOURCES = src/plugins/uptime/uptime.c
uptime_la_CFLAGS = $(AM_CFLAGS)
uptime_la_LDFLAGS = $(PLUGIN_LDFLAGS)
uptime_la_LIBADD =
if BUILD_WITH_LIBKSTAT
uptime_la_LIBADD += -lkstat
endif
if BUILD_WITH_PERFSTAT
uptime_la_LIBADD += -lperfstat
endif
