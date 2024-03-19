include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBRDKAFKA QUIET rdkafka)

find_path(LIBRDKAFKA_INCLUDE_DIR NAMES librdkafka/rdkafka.h
          HINTS ${PC_LIBRDKAFKA_INCLUDEDIR} ${PC_LIBRDKAFKA_INCLUDE_DIRS})
find_library(LIBRDKAFKA_LIBRARIES NAMES rdkafka
             HINTS ${PC_LIBRDKAFKA_LIBDIR} ${PC_LIBRDKAFKA_LIBRARY_DIRS})

check_library_exists(rdkafka "rd_kafka_conf_set_log_cb" "${LIBRDKAFKA_LIBRARIES}" HAVE_LIBRDKAFKA_LOG_CB)
if(HAVE_LIBRDKAFKA_LOG_CB)
    set(LIBRDKAFKA_HAVE_LIBRDKAFKA_LOG_CB ON CACHE BOOL "Log facility is present and usable.")
endif()

check_library_exists(rdkafka "rd_kafka_set_logger" "${LIBRDKAFKA_LIBRARIES}" HAVE_LIBRDKAFKA_LOGGER)
if(HAVE_LIBRDKAFKA_LOGGER)
    set(LIBRDKAFKA_HAVE_LIBRDKAFKA_LOGGER ON CACHE BOOL "Log facility is present and usable.")
endif()

find_package_handle_standard_args(LibRdkafka DEFAULT_MSG LIBRDKAFKA_LIBRARIES LIBRDKAFKA_INCLUDE_DIR)

mark_as_advanced(LIBRDKAFKA_INCLUDE_DIR LIBRDKAFKA_LIBRARIES
                 LIBRDKAFKA_HAVE_LIBRDKAFKA_LOG_CB
                 LIBRDKAFKA_HAVE_LIBRDKAFKA_LOGGER)

if(LIBRDKAFKA_FOUND AND NOT TARGET LibRdkafka::LibRdkafka)
    set(LIBRDKAFKA_INCLUDE_DIRS "${LIBRDKAFKA_INCLUDE_DIR}")
    set(LIBRDKAFKA_DEFINITIONS ${PC_LIBRDKAFKA_CFLAGS_OTHER})
    add_library(LibRdkafka::LibRdkafka INTERFACE IMPORTED)
    set_target_properties(LibRdkafka::LibRdkafka PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBRDKAFKA_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBRDKAFKA_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBRDKAFKA_LIBRARIES}")
endif()
