pkglib_LTLIBRARIES += iptables.la
iptables_la_SOURCES = src/plugins/iptables/iptables.c
iptables_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBIPTC_CPPFLAGS)
iptables_la_LDFLAGS = $(PLUGIN_LDFLAGS)
iptables_la_LIBADD = $(BUILD_WITH_LIBIPTC_LDFLAGS)
