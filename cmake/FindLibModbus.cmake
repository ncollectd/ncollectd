include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBMODBUS QUIET libmodbus)

find_path(LIBMODBUS_INCLUDE_DIR NAMES modbus.h
          HINTS ${PC_LIBMODBUS_INCLUDEDIR} ${PC_LIBMODBUS_INCLUDE_DIRS})
find_library(LIBMODBUS_LIBRARIES NAMES modbus
             HINTS ${PC_LIBMODBUS_LIBDIR} ${PC_LIBMODBUS_LIBRARY_DIRS})

find_package_handle_standard_args(LibModbus DEFAULT_MSG LIBMODBUS_LIBRARIES LIBMODBUS_INCLUDE_DIR)

mark_as_advanced(LIBMODBUS_INCLUDE_DIR LIBMODBUS_LIBRARIES)

if(LIBMODBUS_FOUND AND NOT TARGET LibModbus::LibModbus)
    set(LIBMODBUS_INCLUDE_DIRS "${LIBMODBUS_INCLUDE_DIR}")
    set(LIBMODBUS_DEFINITIONS ${PC_LIBMODBUS_CFLAGS_OTHER})
    add_library(LibModbus::LibModbus INTERFACE IMPORTED)
    set_target_properties(LibModbus::LibModbus PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBMODBUS_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBMODBUS_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBMODBUS_LIBRARIES}")
endif()
