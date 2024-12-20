include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBEPICS QUIET epics-base)

find_path(LIBEPICS_INCLUDE_DIR NAMES cadef.h
          HINTS ${PC_LIBEPICS_INCLUDEDIR} ${PC_LIBEPICS_INCLUDE_DIRS})
find_library(LIBEPICS_LIBRARIES NAMES ca 
             HINTS ${PC_LIBEPICS_LIBDIR} ${PC_LIBEPICS_LIBRARY_DIRS})

find_package_handle_standard_args(LibEpics DEFAULT_MSG LIBEPICS_LIBRARIES LIBEPICS_INCLUDE_DIR)

mark_as_advanced(LIBEPICS_INCLUDE_DIR LIBEPICS_LIBRARIES)

if(LIBEPICS_FOUND AND NOT TARGET LibEpics::LibEpics)
    set(LIBEPICS_INCLUDE_DIRS "${LIBEPICS_INCLUDE_DIR}")
    set(LIBEPICS_DEFINITIONS ${PC_LIBEPICS_CFLAGS_OTHER})
    add_library(LibEpics::LibEpics INTERFACE IMPORTED)
    set_target_properties(LibEpics::LibEpics PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBEPICS_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBEPICS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBEPICS_LIBRARIES}")
endif()
