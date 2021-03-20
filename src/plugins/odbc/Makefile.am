pkglib_LTLIBRARIES += odbc.la
odbc_la_SOURCES = \
	src/plugins/odbc/odbc.c \
	src/plugins/odbc/odbc_query.c \
	src/plugins/odbc/odbc_query.h
odbc_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_UNIXODBC_CPPFLAGS)
odbc_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_UNIXODBC_LDFLAGS)
odbc_la_LIBADD = $(BUILD_WITH_UNIXODBC_LIBS)
