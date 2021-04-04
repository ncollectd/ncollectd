pkglib_LTLIBRARIES += squid.la
squid_la_SOURCES = src/plugins/squid/squid.c 
squid_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
squid_la_LDFLAGS = $(PLUGIN_LDFLAGS)
squid_la_LIBADD = $(BUILD_WITH_LIBCURL_LIBS)
