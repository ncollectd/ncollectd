include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBNETSNMP QUIET netsnmp)

find_path(LIBNETSNMP_INCLUDE_DIR NAMES net-snmp/net-snmp-config.h net-snmp/net-snmp-includes.h
          HINTS ${PC_LIBNETSNMP_INCLUDEDIR} ${PC_LIBNETSNMP_INCLUDE_DIRS})
find_library(LIBNETSNMP_LIBRARIES NAMES netsnmp
             HINTS ${PC_LIBNETSNMP_LIBDIR} ${PC_LIBNETSNMP_LIBRARY_DIRS})

find_package_handle_standard_args(LibNetsnmp DEFAULT_MSG LIBNETSNMP_LIBRARIES LIBNETSNMP_INCLUDE_DIR)

mark_as_advanced(LIBNETSNMP_INCLUDE_DIR LIBNETSNMP_LIBRARIES)

if(LIBNETSNMP_FOUND AND NOT TARGET LibNetsnmp::LibNetsnmp)
    set(LIBNETSNMP_INCLUDE_DIRS "${LIBNETSNMP_INCLUDE_DIR}")
    set(LIBNETSNMP_DEFINITIONS ${PC_LIBNETSNMP_CFLAGS_OTHER})
    add_library(LibNetsnmp::LibNetsnmp INTERFACE IMPORTED)
    set_target_properties(LibNetsnmp::LibNetsnmp PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBNETSNMP_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBNETSNMP_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBNETSNMP_LIBRARIES}")
endif()
