pkglib_LTLIBRARIES += nginx.la
nginx_la_SOURCES = src/plugins/nginx/nginx.c
nginx_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
nginx_la_LDFLAGS = $(PLUGIN_LDFLAGS)
nginx_la_LIBADD = $(BUILD_WITH_LIBCURL_LIBS)
