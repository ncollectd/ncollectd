pkglib_LTLIBRARIES += users.la
users_la_SOURCES = src/plugins/users/users.c
users_la_CFLAGS = $(AM_CFLAGS)
users_la_LDFLAGS = $(PLUGIN_LDFLAGS)
users_la_LIBADD =
if BUILD_WITH_LIBSTATGRAB
users_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
users_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
endif
