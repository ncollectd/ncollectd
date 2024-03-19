include(FindPackageHandleStandardArgs)

find_path(LIBKVM_INCLUDE_DIR NAMES kvm.h)
find_library(LIBKVM_LIBRARIES NAMES kvm)

find_package_handle_standard_args(LibKvm DEFAULT_MSG LIBKVM_LIBRARIES LIBKVM_INCLUDE_DIR)

mark_as_advanced(LIBKVM_INCLUDE_DIR LIBKVM_LIBRARIES)

if(LIBKVM_FOUND AND NOT TARGET LibKvm::LibKvm)
    set(LIBKVM_INCLUDE_DIRS "${LIBKVM_INCLUDE_DIR}")
    add_library(LibKvm::LibKvm INTERFACE IMPORTED)
    set_target_properties(LibKvm::LibKvm PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBKVM_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBKVM_LIBRARIES}")
endif()
