include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBPQ QUIET libpq)

find_path(LIBPQ_INCLUDE_DIR NAMES libpq-fe.h
          HINTS ${PC_LIBPQ_INCLUDEDIR} ${PC_LIBPQ_INCLUDE_DIRS})
find_library(LIBPQ_LIBRARIES NAMES pq
             HINTS ${PC_LIBPQ_LIBDIR} ${PC_LIBPQ_LIBRARY_DIRS})

find_package_handle_standard_args(LibPq DEFAULT_MSG LIBPQ_LIBRARIES LIBPQ_INCLUDE_DIR)

mark_as_advanced(LIBPQ_INCLUDE_DIR LIBPQ_LIBRARIES)

if(LIBPQ_FOUND AND NOT TARGET LibPq::LibPq)
    set(LIBPQ_INCLUDE_DIRS "${LIBPQ_INCLUDE_DIR}")
    set(LIBPQ_DEFINITIONS ${PC_LIBPQ_CFLAGS_OTHER})
    add_library(LibPq::LibPq INTERFACE IMPORTED)
    set_target_properties(LibPq::LibPq PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBPQ_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBPQ_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBPQ_LIBRARIES}")
endif()
