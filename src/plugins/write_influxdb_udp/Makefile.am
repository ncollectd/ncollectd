pkglib_LTLIBRARIES += write_influxdb_udp.la
write_influxdb_udp_la_SOURCES = src/plugins/write_influxdb_udp/write_influxdb_udp.c
write_influxdb_udp_la_CPPFLAGS = $(AM_CPPFLAGS)
write_influxdb_udp_la_LDFLAGS = $(PLUGIN_LDFLAGS)
if BUILD_WITH_LIBSOCKET
write_influxdb_udp_la_LIBADD = -lsocket
endif
