pkglib_LTLIBRARIES += write_prometheus.la
write_prometheus_la_SOURCES = src/plugins/write_prometheus/write_prometheus.c
write_prometheus_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBMICROHTTPD_CPPFLAGS)
write_prometheus_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBMICROHTTPD_LDFLAGS)
write_prometheus_la_LIBADD = $(BUILD_WITH_LIBMICROHTTPD_LIBS)
