pkglib_LTLIBRARIES += haproxy.la
haproxy_la_SOURCES = src/plugins/haproxy/haproxy.c
haproxy_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
haproxy_la_LDFLAGS = $(PLUGIN_LDFLAGS)
haproxy_la_LIBADD = $(BUILD_WITH_LIBCURL_LIBS)
