pkglib_LTLIBRARIES += ntpd.la
ntpd_la_SOURCES = src/plugins/ntpd/ntpd.c
ntpd_la_LDFLAGS = $(PLUGIN_LDFLAGS)
ntpd_la_LIBADD =
if BUILD_WITH_LIBSOCKET
ntpd_la_LIBADD += -lsocket
endif
