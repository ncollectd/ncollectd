pkglib_LTLIBRARIES += swap.la
swap_la_SOURCES = src/plugins/swap/swap.c
swap_la_CFLAGS = $(AM_CFLAGS)
swap_la_LDFLAGS = $(PLUGIN_LDFLAGS)
swap_la_LIBADD =
if BUILD_WITH_LIBKSTAT
swap_la_LIBADD += -lkstat
endif
if BUILD_WITH_LIBDEVINFO
swap_la_LIBADD += -ldevinfo
endif
if BUILD_WITH_LIBKVM_GETSWAPINFO
swap_la_LIBADD += -lkvm
endif
if BUILD_WITH_LIBSTATGRAB
swap_la_CFLAGS += $(BUILD_WITH_LIBSTATGRAB_CFLAGS)
swap_la_LIBADD += $(BUILD_WITH_LIBSTATGRAB_LDFLAGS)
endif
if BUILD_WITH_PERFSTAT
swap_la_LIBADD += -lperfstat
endif

