include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)

pkg_check_modules(PC_LIBBSON QUIET "libbson-1.0")
if (NOT PC_LIBBSON_FOUND)
    pkg_check_modules(PC_LIBBSON QUIET "bson2")

find_path(LIBBSON_INCLUDE_DIR NAMES "bson/bson.h"
          HINTS ${PC_LIBBSON_INCLUDEDIR} ${PC_LIBBSON_INCLUDE_DIRS})
find_library(LIBBSON_LIBRARIES NAMES "bson-1.0" "bson2"
             HINTS ${PC_LIBBSON_LIBDIR} ${PC_LIBBSON_LIBRARY_DIRS})

find_package_handle_standard_args(LibBson DEFAULT_MSG LIBBSON_LIBRARIES LIBBSON_INCLUDE_DIR)

mark_as_advanced(LIBBSON_INCLUDE_DIR LIBBSON_LIBRARIES)

if(LIBBSON_FOUND AND NOT TARGET LibBson::LibBson)
    set(LIBBSON_INCLUDE_DIRS "${LIBBSON_INCLUDE_DIR}")
    set(LIBBSON_DEFINITIONS ${PC_LIBBSON_CFLAGS_OTHER})
    add_library(LibBson::LibBson INTERFACE IMPORTED)
    set_target_properties(LibBson::LibBson PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBBSON_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBBSON_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBBSON_LIBRARIES}")
endif()
