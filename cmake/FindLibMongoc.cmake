include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)

pkg_check_modules(PC_LIBMONGOC QUIET "libmongoc-1.0")
if(NOT PC_LIBMONGOC_FOUND)
    pkg_check_modules(PC_LIBMONGOC QUIET "mongoc2")
endif()

find_path(LIBMONGOC_INCLUDE_SUBDIR NAMES "mongoc/mongoc.h"
          HINTS ${PC_LIBMONGOC_INCLUDEDIR} ${PC_LIBMONGOC_INCLUDE_DIRS})
if(NOT LIBMONGOC_INCLUDE_SUBDIR)
    find_path(LIBMONGOC_INCLUDE_DIR NAMES "mongoc.h" 
          HINTS ${PC_LIBMONGOC_INCLUDEDIR} ${PC_LIBMONGOC_INCLUDE_DIRS})
    if (LIBMONGOC_INCLUDE_DIR)
        set(HAVE_MONGOC_H ON)
    endif()
else()
    set(LIBMONGOC_INCLUDE_DIR ${LIBMONGOC_INCLUDE_SUBDIR})
    set(HAVE_MONGOC_MONGOC_H ON)
endif()

find_library(LIBMONGOC_LIBRARIES NAMES "mongoc-1.0" "mongoc2"
             HINTS ${PC_LIBMONGOC_LIBDIR} ${PC_LIBMONGOC_LIBRARY_DIRS})

find_package_handle_standard_args(LibMongoc DEFAULT_MSG LIBMONGOC_LIBRARIES LIBMONGOC_INCLUDE_DIR)

mark_as_advanced(LIBMONGOC_INCLUDE_DIR LIBMONGOC_LIBRARIES)

if(LIBMONGOC_FOUND AND NOT TARGET LibMongoc::LibMongoc)
    set(LIBMONGOC_INCLUDE_DIRS "${LIBMONGOC_INCLUDE_DIR}")
    if (HAVE_MONGOC_MONGOC_H)
        set(LIBMONGOC_DEFINITIONS HAVE_MONGOC_MONGOC_H)
    elseif(HAVE_MONGOC_H)
        set(LIBMONGOC_DEFINITIONS HAVE_MONGOC_H)
    endif()
    
    add_library(LibMongoc::LibMongoc INTERFACE IMPORTED)
    set_target_properties(LibMongoc::LibMongoc PROPERTIES
                          INTERFACE_COMPILE_DEFINITIONS "${LIBMONGOC_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBMONGOC_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBMONGOC_LIBRARIES}")
endif()
