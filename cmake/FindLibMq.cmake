include(FindPackageHandleStandardArgs)

find_path(LIBMQ_INCLUDE_DIR NAMES cmqc.h PATH_SUFFIXES inc)
find_library(LIBMQ_LIBRARIES NAMES mqm_r mqic_r)

find_package_handle_standard_args(LibMq DEFAULT_MSG LIBMQ_LIBRARIES LIBMQ_INCLUDE_DIR)

mark_as_advanced(LIBMQ_INCLUDE_DIR LIBMQ_LIBRARIES)

if(LIBMQ_FOUND AND NOT TARGET LibMq::LibMq)
    set(LIBMQ_INCLUDE_DIRS "${LIBMQ_INCLUDE_DIR}")
    add_library(LibMq::LibMq INTERFACE IMPORTED)
    set_target_properties(LibMq::LibMq PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBMQ_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBMQ_LIBRARIES}")
endif()
