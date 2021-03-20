pkglib_LTLIBRARIES += modbus.la
modbus_la_SOURCES = src/plugins/modbus/modbus.c
modbus_la_CFLAGS = $(AM_CFLAGS) $(BUILD_WITH_LIBMODBUS_CFLAGS)
modbus_la_LDFLAGS = $(PLUGIN_LDFLAGS)
modbus_la_LIBADD = $(BUILD_WITH_LIBMODBUS_LIBS)
