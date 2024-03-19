include(FindPackageHandleStandardArgs)

find_path(LIBGEOM_INCLUDE_DIR NAMES libgeom.h)
find_library(LIBGEOM_LIBRARIES NAMES geom)

find_package_handle_standard_args(LibGeom DEFAULT_MSG LIBGEOM_LIBRARIES LIBGEOM_INCLUDE_DIR)

mark_as_advanced(LIBGEOM_INCLUDE_DIR LIBGEOM_LIBRARIES)

if(LIBGEOM_FOUND AND NOT TARGET LibGeom::LibGeom)
    set(LIBGEOM_INCLUDE_DIRS "${LIBGEOM_INCLUDE_DIR}")
    add_library(LibGeom::LibGeom INTERFACE IMPORTED)
    set_target_properties(LibGeom::LibGeom PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBGEOM_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBGEOM_LIBRARIES}")
endif()
