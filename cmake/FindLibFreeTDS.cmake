include(FindPackageHandleStandardArgs)

find_path(LIBFREETDS_INCLUDE_DIR NAMES sqlfront.h sqldb.h)
find_library(LIBFREETDS_LIBRARIES NAMES sybdb)

find_package_handle_standard_args(LibFreeTDS DEFAULT_MSG LIBFREETDS_LIBRARIES LIBFREETDS_INCLUDE_DIR)

mark_as_advanced(LIBFREETDS_INCLUDE_DIR LIBFREETDS_LIBRARIES)

if(LIBFREETDS_FOUND AND NOT TARGET LibFreeTDS::LibFreeTDS)
    set(LIBFREETDS_INCLUDE_DIRS "${LIBFREETDS_INCLUDE_DIR}")
    add_library(LibFreeTDS::LibFreeTDS INTERFACE IMPORTED)
    set_target_properties(LibFreeTDS::LibFreeTDS PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBFREETDS_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBFREETDS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBFREETDS_LIBRARIES}")
endif()
