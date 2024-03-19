include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSLURM QUIET slurm)

find_path(LIBSLURM_INCLUDE_DIR NAMES slurm/slurm.h
          HINTS ${PC_LIBSLURM_INCLUDEDIR} ${PC_LIBSLURM_INCLUDE_DIRS})
find_library(LIBSLURM_LIBRARIES NAMES slurm
             HINTS ${PC_LIBSLURM_LIBDIR} ${PC_LIBSLURM_LIBRARY_DIRS})

find_package_handle_standard_args(LibSlurm DEFAULT_MSG LIBSLURM_LIBRARIES LIBSLURM_INCLUDE_DIR)

mark_as_advanced(LIBSLURM_INCLUDE_DIR LIBSLURM_LIBRARIES)

if(LIBSLURM_FOUND AND NOT TARGET LibSlurm::LibSlurm)
    set(LIBSLURM_INCLUDE_DIRS "${LIBSLURM_INCLUDE_DIR}")
    set(LIBSLURM_DEFINITIONS ${PC_LIBSLURM_CFLAGS_OTHER})
    add_library(LibSlurm::LibSlurm INTERFACE IMPORTED)
    set_target_properties(LibSlurm::LibSlurm PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBSLURM_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBSLURM_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBSLURM_LIBRARIES}")
endif()
