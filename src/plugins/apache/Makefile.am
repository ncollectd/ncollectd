pkglib_LTLIBRARIES += apache.la
apache_la_SOURCES = src/plugins/apache/apache.c
apache_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
apache_la_LDFLAGS = $(PLUGIN_LDFLAGS)
apache_la_LIBADD = $(BUILD_WITH_LIBCURL_LIBS)
