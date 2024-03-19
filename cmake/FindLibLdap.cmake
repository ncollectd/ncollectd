include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)

pkg_check_modules(PC_LIBLDAP QUIET libldap)

find_path(LIBLDAP_INCLUDE_DIR NAMES ldap.h)

find_library(LDAP_LIBRARY NAMES ldap
             HINTS ${PC_LIBLDAP_INCLUDEDIR} ${PC_LIBLDAP_INCLUDE_DIRS})
find_library(LBER_LIBRARY NAMES lber
             HINTS ${PC_LIBLDAP_INCLUDEDIR} ${PC_LIBLDAP_INCLUDE_DIRS})

if(LDAP_LIBRARY AND LBER_LIBRARY)
    set(LIBLDAP_LIBRARIES ${LDAP_LIBRARY} ${LBER_LIBRARY})
endif()

find_package_handle_standard_args(LibLdap DEFAULT_MSG LIBLDAP_LIBRARIES LIBLDAP_INCLUDE_DIR)

mark_as_advanced(LIBLDAP_INCLUDE_DIR LIBLDAP_LIBRARIES)

if(LIBLDAP_FOUND AND NOT TARGET LibLdap::LibLdap)
    set(LIBLDAP_INCLUDE_DIRS "${LIBLDAP_INCLUDE_DIR}")
    set(LIBLDAP_DEFINITIONS ${PC_LIBLDAP_CFLAGS_OTHER})
    add_library(LibLdap::LibLdap INTERFACE IMPORTED)
    set_target_properties(LibLdap::LibLdap PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBLDAP_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBLDAP_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBLDAP_LIBRARIES}")
endif()
