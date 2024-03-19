set(LIBCONFIG_SCANNER_H "${CMAKE_CURRENT_BINARY_DIR}/scanner.h")
set(LIBCONFIG_SCANNER_C "${CMAKE_CURRENT_BINARY_DIR}/scanner.c")
set(LIBCONFIG_PARSER_H "${CMAKE_CURRENT_BINARY_DIR}/parser.h")
set(LIBCONFIG_PARSER_C "${CMAKE_CURRENT_BINARY_DIR}/parser.c")

set(LIBCONFIG_SRC config.c config.h
                  aux_types.h
                  "${LIBCONFIG_SCANNER_C}" "${LIBCONFIG_SCANNER_H}"
                  "${LIBCONFIG_PARSER_C}" "${LIBCONFIG_PARSER_H}")

flex_target(LIBCONFIG_SCANNER "${CMAKE_CURRENT_SOURCE_DIR}/scanner.l"
            "${LIBCONFIG_SCANNER_C}" DEFINES_FILE "${LIBCONFIG_SCANNER_H}")
bison_target(LIBCONFIG_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/parser.y"
             "${LIBCONFIG_PARSER_C}" DEFINES_FILE "${LIBCONFIG_PARSER_H}")
add_flex_bison_dependency(LIBCONFIG_SCANNER LIBCONFIG_PARSER)

add_library(libconfig STATIC ${LIBCONFIG_SRC})
set_target_properties(libconfig PROPERTIES PREFIX "")
set_target_properties(libconfig PROPERTIES POSITION_INDEPENDENT_CODE ON)
#add_dependencies(libconfig libutils)
