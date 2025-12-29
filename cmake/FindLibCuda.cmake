include(FindPackageHandleStandardArgs)

find_path(LIBCUDA_INCLUDE_DIR NAMES nvml.h)
find_library(LIBCUDA_LIBRARIES NAMES nvidia-ml PATH_SUFFIXES "lib64/stubs")

find_package_handle_standard_args(LibCuda DEFAULT_MSG LIBCUDA_LIBRARIES LIBCUDA_INCLUDE_DIR)

mark_as_advanced(LIBCUDA_INCLUDE_DIR LIBCUDA_LIBRARIES)

if(LIBCUDA_FOUND AND NOT TARGET LibCuda::LibCuda)
    set(LIBCUDA_INCLUDE_DIRS "${LIBCUDA_INCLUDE_DIR}")
    add_library(LibCuda::LibCuda INTERFACE IMPORTED)
    set_target_properties(LibCuda::LibCuda PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBCUDA_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBCUDA_LIBRARIES}")
endif()
