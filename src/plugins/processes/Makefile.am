pkglib_LTLIBRARIES += processes.la
processes_la_SOURCES = src/plugins/processes/processes.c
processes_la_CPPFLAGS = $(AM_CPPFLAGS)
processes_la_LDFLAGS = $(PLUGIN_LDFLAGS)
processes_la_LIBADD =
if BUILD_WITH_LIBKVM_GETPROCS
processes_la_LIBADD += -lkvm
endif
if HAVE_LIBMNL
processes_la_CPPFLAGS += -DHAVE_LIBTASKSTATS=1
processes_la_LIBADD += libtaskstats.la
endif
