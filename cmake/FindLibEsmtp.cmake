include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBESMTP QUIET libesmtp)

find_path(LIBESMTP_INCLUDE_DIR NAMES libesmtp.h
          HINTS ${PC_LIBESMTP_INCLUDEDIR} ${PC_LIBESMTP_INCLUDE_DIRS})
find_library(LIBESMTP_LIBRARIES NAMES esmtp
             HINTS ${PC_LIBESMTP_LIBDIR} ${PC_LIBESMTP_LIBRARY_DIRS})

find_package_handle_standard_args(LibEsmtp DEFAULT_MSG LIBESMTP_LIBRARIES LIBESMTP_INCLUDE_DIR)

mark_as_advanced(LIBESMTP_INCLUDE_DIR LIBESMTP_LIBRARIES)

if(LIBESMTP_FOUND AND NOT TARGET LibEsmtp::LibEsmtp)
    set(LIBESMTP_INCLUDE_DIRS "${LIBESMTP_INCLUDE_DIR}")
    set(LIBESMTP_DEFINITIONS ${PC_LIBESMTP_CFLAGS_OTHER})
    add_library(LibEsmtp::LibEsmtp INTERFACE IMPORTED)
    set_target_properties(LibEsmtp::LibEsmtp PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBESMTP_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBESMTP_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBESMTP_LIBRARIES}")
endif()
