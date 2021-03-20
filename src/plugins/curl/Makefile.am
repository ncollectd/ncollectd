pkglib_LTLIBRARIES += curl.la
curl_la_SOURCES = \
	src/plugins/curl/curl.c \
	src/utils/curl_stats/curl_stats.c \
	src/utils/curl_stats/curl_stats.h \
	src/utils/match/match.c \
	src/utils/match/match.h
curl_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBCURL_CFLAGS)
curl_la_LDFLAGS = $(PLUGIN_LDFLAGS)
curl_la_LIBADD = liblatency.la $(BUILD_WITH_LIBCURL_LIBS)
