pkglib_LTLIBRARIES += ping.la
ping_la_SOURCES = src/plugins/ping/ping.c
ping_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBOPING_CPPFLAGS)
ping_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBOPING_LDFLAGS)
ping_la_LIBADD = -loping -lm
