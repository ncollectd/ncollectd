pkglib_LTLIBRARIES += log_logstash.la
log_logstash_la_SOURCES = src/plugins/log_logstash/log_logstash.c
log_logstash_la_CPPFLAGS = $(AM_CPPFLAGS) $(BUILD_WITH_LIBYAJL_CPPFLAGS)
log_logstash_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(BUILD_WITH_LIBYAJL_LDFLAGS)
log_logstash_la_LIBADD = $(BUILD_WITH_LIBYAJL_LIBS)
