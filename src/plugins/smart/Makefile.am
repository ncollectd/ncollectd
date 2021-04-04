if BUILD_WITH_LIBUDEV
pkglib_LTLIBRARIES += smart.la
smart_la_SOURCES = \
    src/plugins/smart/smart.c \
    src/plugins/smart/nvme.h \
    src/plugins/smart/intel-nvme.h
smart_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBATASMART_CPPFLAGS) $(BUILD_WITH_LIBUDEV_CPPFLAGS)
smart_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBATASMART_LDFLAGS) $(BUILD_WITH_LIBUDEV_LDFLAGS)
smart_la_LIBADD = libignorelist.la $(BUILD_WITH_LIBATASMART_LIBS) $(BUILD_WITH_LIBUDEV_LIBS)

test_plugin_smart_SOURCES = src/smart_test.c
test_plugin_smart_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBATASMART_CPPFLAGS) $(BUILD_WITH_LIBUDEV_CPPFLAGS)
test_plugin_smart_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBATASMART_LDFLAGS) $(BUILD_WITH_LIBUDEV_LDFLAGS)
test_plugin_smart_LDADD = libplugin_mock.la $(BUILD_WITH_LIBATASMART_LIBS) $(BUILD_WITH_LIBUDEV_LIBS)
check_PROGRAMS += test_plugin_smart
TESTS += test_plugin_smart
endif
