pkglib_LTLIBRARIES += varnish.la
varnish_la_SOURCES = src/plugins/varnish/varnish.c
varnish_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBVARNISH_CFLAGS)
varnish_la_LDFLAGS = $(PLUGIN_LDFLAGS)
varnish_la_LIBADD = $(BUILD_WITH_LIBVARNISH_LIBS)
