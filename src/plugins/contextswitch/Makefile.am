pkglib_LTLIBRARIES += contextswitch.la
contextswitch_la_SOURCES = src/plugins/contextswitch/contextswitch.c
contextswitch_la_LDFLAGS = $(PLUGIN_LDFLAGS)
contextswitch_la_LIBADD =
if BUILD_WITH_PERFSTAT
contextswitch_la_LIBADD += -lperfstat
endif
