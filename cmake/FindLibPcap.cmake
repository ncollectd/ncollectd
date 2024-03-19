include(FindPackageHandleStandardArgs)
include(CheckSymbolExists)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBPCAP QUIET pcap)

find_path(LIBPCAP_INCLUDE_DIR NAMES pcap.h
          HINTS ${PC_LIBPCAP_INCLUDEDIR} ${PC_LIBPCAP_INCLUDE_DIRS})
find_library(LIBPCAP_LIBRARIES NAMES pcap
             HINTS ${PC_LIBPCAP_LIBDIR} ${PC_LIBPCAP_LIBRARY_DIRS})

check_symbol_exists("PCAP_ERROR_IFACE_NOT_UP" "pcap.h" HAVE_PCAP_ERROR_IFACE_NOT_UP)

find_package_handle_standard_args(LibPcap DEFAULT_MSG LIBPCAP_LIBRARIES LIBPCAP_INCLUDE_DIR)

mark_as_advanced(LIBPCAP_INCLUDE_DIR LIBPCAP_LIBRARIES)

if(LIBPCAP_FOUND AND NOT TARGET LibPcap::LibPcap)
    set(LIBPCAP_INCLUDE_DIRS "${LIBPCAP_INCLUDE_DIR}")
    set(LIBPCAP_DEFINITIONS ${PC_LIBPCAP_CFLAGS_OTHER})
    add_library(LibPcap::LibPcap INTERFACE IMPORTED)
    set_target_properties(LibPcap::LibPcap PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBPCAP_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBPCAP_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBPCAP_LIBRARIES}")
endif()
