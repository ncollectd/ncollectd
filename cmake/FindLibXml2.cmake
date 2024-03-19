include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBXML QUIET libxml-2.0)

find_path(LIBXML2_INCLUDE_DIR NAMES libxml/xpath.h
          HINTS ${PC_LIBXML_INCLUDEDIR} ${PC_LIBXML_INCLUDE_DIRS}
          PATH_SUFFIXES libxml2)
find_library(LIBXML2_LIBRARIES NAMES xml2
             HINTS ${PC_LIBXML_LIBDIR} ${PC_LIBXML_LIBRARY_DIRS})

find_package_handle_standard_args(LibXml2 REQUIRED_VARS LIBXML2_LIBRARIES LIBXML2_INCLUDE_DIR)

mark_as_advanced(LIBXML2_INCLUDE_DIR LIBXML2_LIBRARIES)

if(LIBXML2_FOUND AND NOT TARGET LibXml2::LibXml2)
    set(LIBXML2_INCLUDE_DIRS "${LIBXML2_INCLUDE_DIR}")
    set(LIBXML2_DEFINITIONS ${PC_LIBXML_CFLAGS_OTHER})
    add_library(LibXml2::LibXml2 INTERFACE IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBXML2_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBXML2_LIBRARIES}")
endif()
