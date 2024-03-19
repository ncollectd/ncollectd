include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBCUPS QUIET cups)

find_path(LIBCUPS_INCLUDE_DIR cups/cups.h
          HINTS ${PC_LIBCUPS_INCLUDEDIR} ${PC_LIBCUPS_INCLUDE_DIRS})
find_library(LIBCUPS_LIBRARIES NAMES cups
             HINTS ${PC_LIBCUPS_LIBDIR} ${PC_LIBCUPS_LIBRARY_DIRS})

if(PC_LIBCUPS_VERSION)
    set(LIBCUPS_VERSION "${PC_LIBCUPS_VERSION}")
else()
    if (LIBCUPS_INCLUDE_DIR AND EXISTS "${LIBCUPS_INCLUDE_DIR}/cups/cups.h")
        file(STRINGS "${LIBCUPS_INCLUDE_DIR}/cups/cups.h" LIBCUPS_VERSION_STR
             REGEX "^#[\t ]*define[\t ]+CUPS_VERSION_(MAJOR|MINOR|PATCH)[\t ]+[0-9]+$")
        unset(LIBCUPS_VERSION)
        foreach(VPART MAJOR MINOR PATCH)
            foreach(VLINE ${LIBCUPS_VERSION_STR})
                if(VLINE MATCHES "^#[\t ]*define[\t ]+CUPS_VERSION_${VPART}[\t ]+([0-9]+)$")
                    set(LIBCUPS_VERSION_PART "${CMAKE_MATCH_1}")
                    if(LIBCUPS_VERSION)
                        string(APPEND LIBCUPS_VERSION ".${LIBCUPS_VERSION_PART}")
                    else()
                        set(LIBCUPS_VERSION "${LIBCUPS_VERSION_PART}")
                    endif()
                endif()
            endforeach()
        endforeach()
    endif ()
endif()

find_package_handle_standard_args(LibCups REQUIRED_VARS LIBCUPS_LIBRARIES LIBCUPS_INCLUDE_DIR
                                          VERSION_VAR LIBCUPS_VERSION)

mark_as_advanced(LIBCUPS_INCLUDE_DIR LIBCUPS_LIBRARIES LIBCUPS_VERSION)

if(LIBCUPS_FOUND AND NOT TARGET LibCups::LibCups)
    set(LIBCUPS_INCLUDE_DIRS "${LIBCUPS_INCLUDE_DIR}")
    set(LIBCUPS_DEFINITIONS ${PC_LIBCUPS_CFLAGS_OTHER})
    add_library(LibCups::LibCups INTERFACE IMPORTED)
    set_target_properties(LibCups::LibCups PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBXML2_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBCUPS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBCUPS_LIBRARIES}")
endif()
