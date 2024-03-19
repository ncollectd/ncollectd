include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBATASMART QUIET libatasmart)

find_path(LIBATASMART_INCLUDE_DIR NAMES atasmart.h
          HINTS ${PC_LIBATASMART_INCLUDEDIR} ${PC_LIBATASMART_INCLUDE_DIRS})
find_library(LIBATASMART_LIBRARIES NAMES atasmart
             HINTS ${PC_LIBATASMART_LIBDIR} ${PC_LIBATASMART_LIBRARY_DIRS})

find_package_handle_standard_args(LibAtasmart DEFAULT_MSG LIBATASMART_LIBRARIES LIBATASMART_INCLUDE_DIR)

mark_as_advanced(LIBATASMART_INCLUDE_DIR LIBATASMART_LIBRARIES)

if(LIBATASMART_FOUND AND NOT TARGET LibAtasmart::LibAtasmart)
    set(LIBATASMART_INCLUDE_DIRS "${LIBATASMART_INCLUDE_DIR}")
    set(LIBATASMART_DEFINITIONS ${PC_LIBATASMART_CFLAGS_OTHER})
    if(NOT TARGET LibAtasmart::LibAtasmart)
        add_library(LibAtasmart::LibAtasmart INTERFACE IMPORTED)
        set_target_properties(LibAtasmart::LibAtasmart PROPERTIES
                              INTERFACE_COMPILE_OPTIONS     "${LIBATASMART_DEFINITIONS}"
                              INTERFACE_INCLUDE_DIRECTORIES "${LIBATASMART_INCLUDE_DIRS}"
                              INTERFACE_LINK_LIBRARIES      "${LIBATASMART_LIBRARIES}")
    endif()
endif()
