include(FindPackageHandleStandardArgs)
include(CheckLibraryExists)
include(CheckSymbolExists)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBCAP QUIET libcap)

find_path(LIBCAP_INCLUDE_DIR NAMES sys/capability.h
          HINTS ${PC_LIBCAP_INCLUDEDIR} ${PC_LIBCAP_INCLUDE_DIRS})
find_library(LIBCAP_LIBRARIES NAMES cap
             HINTS ${PC_LIBCAP_LIBDIR} ${PC_LIBCAP_LIBRARY_DIRS})

check_library_exists(cap "cap_get_proc" "${LIBCAP_LIBRARIES}" HAVE_CAP_GET_PROC)
check_symbol_exists(CAP_IS_SUPPORTED "sys/capability.h" HAVE_CAP_IS_SUPPORTED)
if(NOT (HAVE_CAP_GET_PROC AND HAVE_CAP_IS_SUPPORTED))
    unset(LIBCAP_INCLUDE_DIR)
    unset(LIBCAP_INCLUDE_DIR CACHE)
    unset(LIBCAP_LIBRARIES)
    unset(LIBCAP_LIBRARIES CACHE)
endif()

find_package_handle_standard_args(LibCap DEFAULT_MSG LIBCAP_LIBRARIES LIBCAP_INCLUDE_DIR)

mark_as_advanced(LIBCAP_INCLUDE_DIR LIBCAP_LIBRARIES)

if(LIBCAP_FOUND AND NOT TARGET LibCap::LibCap)
    set(LIBCAP_INCLUDE_DIRS "${LIBCAP_INCLUDE_DIR}")
    set(LIBCAP_DEFINITIONS HAVE_CAPABILITY)
    add_library(LibCap::LibCap INTERFACE IMPORTED)
    set_target_properties(LibCap::LibCap PROPERTIES
                          INTERFACE_COMPILE_DEFINITIONS "${LIBCAP_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBCAP_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBCAP_LIBRARIES}")
endif()
