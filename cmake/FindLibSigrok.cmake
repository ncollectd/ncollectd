include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSIGROK QUIET "libsigrok")

find_path(LIBSIGROK_INCLUDE_DIR NAMES "libsigrok/libsigrok.h"
          HINTS ${PC_LIBSIGROK_INCLUDEDIR} ${PC_LIBSIGROK_INCLUDE_DIRS})
find_library(LIBSIGROK_LIBRARIES NAMES "sigrok"
             HINTS ${PC_LIBSIGROK_LIBDIR} ${PC_LIBSIGROK_LIBRARY_DIRS})

if(PC_LIBSIGROK_VERSION)
    if(PC_LIBSIGROK_VERSION VERSION_LESS "0.5.0")
        unset(LIBSIGROK_INCLUDE_DIR)
        unset(LIBSIGROK_INCLUDE_DIR CACHE)
        unset(LIBSIGROK_LIBRARIES)
        unset(LIBSIGROK_LIBRARIES CACHE)
    endif()
endif()

find_package_handle_standard_args(LibSigrok DEFAULT_MSG LIBSIGROK_LIBRARIES LIBSIGROK_INCLUDE_DIR)

mark_as_advanced(LIBSIGROK_INCLUDE_DIR LIBSIGROK_LIBRARIES)

if(LIBSIGROK_FOUND AND NOT TARGET LibSigrok::LibSigrok)
    set(LIBSIGROK_INCLUDE_DIRS "${LIBSIGROK_INCLUDE_DIR}")
    set(LIBSIGROK_DEFINITIONS ${PC_LIBSIGROK_CFLAGS_OTHER})
    add_library(LibSigrok::LibSigrok INTERFACE IMPORTED)
    set_target_properties(LibSigrok::LibSigrok PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBSIGROK_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBSIGROK_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBSIGROK_LIBRARIES}")
endif()
