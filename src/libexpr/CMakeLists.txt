set(LIBEXPR_SCANNER_H "${CMAKE_CURRENT_BINARY_DIR}/scanner.h")
set(LIBEXPR_SCANNER_C "${CMAKE_CURRENT_BINARY_DIR}/scanner.c")
set(LIBEXPR_PARSER_H "${CMAKE_CURRENT_BINARY_DIR}/parser.h")
set(LIBEXPR_PARSER_C "${CMAKE_CURRENT_BINARY_DIR}/parser.c")

set(LIBEXPR_SRC expr.c expr.h
                eval.c
                symtab.c
                "${LIBEXPR_SCANNER_C}" "${LIBEXPR_SCANNER_H}"
                "${LIBEXPR_PARSER_C}" "${LIBEXPR_PARSER_H}")

flex_target(LIBEXPR_SCANNER "${CMAKE_CURRENT_SOURCE_DIR}/scanner.l"
            "${LIBEXPR_SCANNER_C}" DEFINES_FILE "${LIBEXPR_SCANNER_H}")
bison_target(LIBEXPR_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/parser.y"
             "${LIBEXPR_PARSER_C}" DEFINES_FILE "${LIBEXPR_PARSER_H}")
add_flex_bison_dependency(LIBEXPR_SCANNER LIBEXPR_PARSER)

add_library(libexpr STATIC ${LIBEXPR_SRC})
set_target_properties(libexpr PROPERTIES PREFIX "")
set_target_properties(libexpr PROPERTIES POSITION_INDEPENDENT_CODE ON)
#add_dependencies(libexpr libutils)

add_executable(test_libexpr_expr EXCLUDE_FROM_ALL expr_test.c)
target_link_libraries(test_libexpr_expr libexpr libutils libmetric libtest -lm)
add_dependencies(build_tests test_libexpr_expr)
add_test(NAME test_libexpr_expr COMMAND test_libexpr_expr)
