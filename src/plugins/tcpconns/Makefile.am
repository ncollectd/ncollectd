pkglib_LTLIBRARIES += tcpconns.la
tcpconns_la_SOURCES = src/plugins/tcpconns/tcpconns.c
tcpconns_la_LDFLAGS = $(PLUGIN_LDFLAGS)
tcpconns_la_LIBADD =
if BUILD_WITH_LIBKVM_NLIST
tcpconns_la_LIBADD += -lkvm
endif
