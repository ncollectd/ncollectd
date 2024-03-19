include(FindPackageHandleStandardArgs)

if(NOT ORACLE_HOME)
    if(EXISTS $ENV{ORACLE_HOME})
        set(ORACLE_HOME $ENV{ORACLE_HOME})
    endif()
endif()

find_path(LIBOCI_INCLUDE_DIR NAMES oci.h
          PATHS ${ORACLE_HOME}/rdbms/public
                ${ORACLE_HOME}/include
                ${ORACLE_HOME}/sdk/include
                /usr/include/oracle/*/client${LIB_SUFFIX})

find_library(LIBOCI_LIBRARIES NAMES clntsh
             PATHS ${ORACLE_HOME}
                   ${ORACLE_HOME}/lib
                   ${ORACLE_HOME}/sdk/lib
                   /usr/lib/oracle/*/client${LIB_SUFFIX}/lib)

find_package_handle_standard_args(LibOci DEFAULT_MSG LIBOCI_LIBRARIES LIBOCI_INCLUDE_DIR)

mark_as_advanced(LIBOCI_INCLUDE_DIR LIBOCI_LIBRARIES)

if(LIBOCI_FOUND AND NOT TARGET LibOci::LibOci)
    set(LIBOCI_INCLUDE_DIRS "${LIBOCI_INCLUDE_DIR}")
    add_library(LibOci::LibOci INTERFACE IMPORTED)
    set_target_properties(LibOci::LibOci PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBOCI_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBOCI_LIBRARIES}")
endif()
