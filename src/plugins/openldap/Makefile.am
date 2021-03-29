pkglib_LTLIBRARIES += openldap.la
openldap_la_SOURCES = src/plugins/openldap/openldap.c
openldap_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBLDAP_CPPFLAGS)
openldap_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBLDAP_LDFLAGS)
openldap_la_LIBADD = -lldap
