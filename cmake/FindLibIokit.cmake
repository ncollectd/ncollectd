include(FindPackageHandleStandardArgs)
include(CMakeFindFrameworks)

cmake_find_frameworks(IOKit)

if(IOKit_FRAMEWORKS)
   set(LIBIOKIT_LIBRARIES "-framework IOKit")
endif()

find_package_handle_standard_args(LibIokit DEFAULT_MSG LIBIOKIT_LIBRARIES)

mark_as_advanced(LIBIOKIT_LIBRARIES)

if(LIBIOKIT_FOUND AND NOT TARGET LibIokit::LibIokit)
    add_library(LibIokit::LibIokit INTERFACE IMPORTED)
    set_target_properties(LibIokit::LibIokit PROPERTIES
                          INTERFACE_LINK_LIBRARIES      "${LIBIOKIT_LIBRARIES}")
endif()
