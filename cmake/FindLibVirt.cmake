include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBVIRT QUIET libvirt)

find_path(LIBVIRT_INCLUDE_DIR NAMES libvirt/libvirt.h libvirt/virterror.h
          HINTS ${PC_LIBVIRT_INCLUDEDIR} ${PC_LIBVIRT_INCLUDE_DIRS})
find_library(LIBVIRT_LIBRARIES NAMES virt
             HINTS ${PC_LIBVIRT_LIBDIR} ${PC_LIBVIRT_LIBRARY_DIRS})

find_package_handle_standard_args(LibVirt REQUIRED_VARS LIBVIRT_LIBRARIES LIBVIRT_INCLUDE_DIR)

mark_as_advanced(LIBVIRT_INCLUDE_DIR LIBVIRT_LIBRARIES)

if(LIBVIRT_FOUND AND NOT TARGET LibVirt::LibVirt)
    set(LIBVIRT_INCLUDE_DIRS "${LIBVIRT_INCLUDE_DIR}")
    set(LIBVIRT_CFLAGS ${PC_LIBVIRT_CFLAGS_OTHER})
    add_library(LibVirt::LibVirt INTERFACE IMPORTED)
    set_target_properties(LibVirt::LibVirt PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBVIRT_CFLAGS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBVIRT_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBVIRT_LIBRARIES}")
endif()
