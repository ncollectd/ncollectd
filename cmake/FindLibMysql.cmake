include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBMYSQL QUIET libmariadb)

find_path(LIBMYSQL_INCLUDE_SUBDIR NAMES mysql/mysql.h
          HINTS ${PC_LIBMYSQL_INCLUDEDIR} ${PC_LIBMYSQL_INCLUDE_DIRS})
if(NOT LIBMYSQL_INCLUDE_SUBDIR)
    find_path(LIBMYSQL_INCLUDE_DIR NAMES mysql.h
              HINTS ${PC_LIBMYSQL_INCLUDEDIR} ${PC_LIBMYSQL_INCLUDE_DIRS})
    if(LIBMYSQL_INCLUDE_DIR)
        set(HAVE_MYSQL_H ON)
    endif()
else()
    set(LIBMYSQL_INCLUDE_DIR ${LIBMYSQL_INCLUDE_SUBDIR})
    set(HAVE_MYSQL_MYSQL_H ON)
endif()

find_library(LIBMYSQL_LIBRARIES NAMES mysqlclient mariadb
                                PATHS "/lib/mysql"
                                      "/lib64/mysql"
                                      "/usr/lib/mysql"
                                      "/usr/lib64/mysql"
                                      "/usr/local/lib/mysql"
                                      "/usr/local/lib64/mysql"
                                      "/usr/mysql/lib/mysql"
                                      "/usr/mysql/lib64/mysql"
                                HINTS ${PC_LIBMYSQL_LIBDIR} ${PC_LIBMYSQL_LIBRARY_DIRS})

find_package_handle_standard_args(LibMysql DEFAULT_MSG LIBMYSQL_LIBRARIES LIBMYSQL_INCLUDE_DIR)

mark_as_advanced(LIBMYSQL_INCLUDE_DIR LIBMYSQL_LIBRARIES)

if(LIBMYSQL_FOUND AND NOT TARGET LibMysql::LibMysql)
    set(LIBMYSQL_INCLUDE_DIRS "${LIBMYSQL_INCLUDE_DIR}")
    set(LIBMYSQL_CFLAGS ${PC_LIBMYSQL_CFLAGS_OTHER})
    if (HAVE_MYSQL_MYSQL_H)
        set(LIBMYSQL_DEFINITIONS HAVE_MYSQL_MYSQL_H)
    elseif(HAVE_MYSQL_H)
        set(LIBMYSQL_DEFINITIONS HAVE_MYSQL_H)
    endif()
    add_library(LibMysql::LibMysql INTERFACE IMPORTED)
    set_target_properties(LibMysql::LibMysql PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBMYSQL_CFLAGS}"
                          INTERFACE_COMPILE_DEFINITIONS "${LIBMYSQL_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBMYSQL_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBMYSQL_LIBRARIES}")
endif()
