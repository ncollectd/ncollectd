include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBSSL QUIET openssl)

find_path(LIBSSL_INCLUDE_DIR NAMES openssl/sha.h openssl/blowfish.h openssl/rand.h
          HINTS ${PC_LIBSSL_INCLUDEDIR} ${PC_LIBSSL_INCLUDE_DIRS})
find_library(LIBSSL_LIBRARIES NAMES ssl crypto
             HINTS ${PC_LIBSSL_LIBDIR} ${PC_LIBSSL_LIBRARY_DIRS})

set(LIBSSL_VERSION "${PC_LIBSSL_VERSION}")

find_package_handle_standard_args(LibSsl REQUIRED_VARS LIBSSL_LIBRARIES LIBSSL_INCLUDE_DIR
                                         VERSION_VAR LIBSSL_VERSION)

mark_as_advanced(LIBSSL_INCLUDE_DIR LIBSSL_LIBRARIES LIBSSL_VERSION)

if(LIBSSL_FOUND AND NOT TARGET LibSsl::LibSsl)
    set(LIBSSL_INCLUDE_DIRS "${LIBSSL_INCLUDE_DIR}")
    set(LIBSSL_DEFINITIONS ${PC_LIBSSL_CFLAGS_OTHER})
    add_library(LibSsl::LibSsl INTERFACE IMPORTED)
    set_target_properties(LibSsl::LibSsl PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBSSL_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBSSL_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBSSL_LIBRARIES}")
endif()
