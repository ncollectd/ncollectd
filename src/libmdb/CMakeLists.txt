set(LIBMDB_SCANNER_H "${CMAKE_CURRENT_BINARY_DIR}/scanner.h")
set(LIBMDB_SCANNER_C "${CMAKE_CURRENT_BINARY_DIR}/scanner.c")
set(LIBMDB_PARSER_H "${CMAKE_CURRENT_BINARY_DIR}/parser.h")
set(LIBMDB_PARSER_C "${CMAKE_CURRENT_BINARY_DIR}/parser.c")

set(LIBMDB_SRC mdb.c mdb.h
               crc32c.c crc32c.h
               metric_id.c metric_id.h
               index.c index.h
               rindex.c rindex.h
               family.c family.h
               functions.c functions.h
               eval.c
               node.c node.h
               util.c util.h
               value.c value.h
               table.c table.h
               strlist.c strlist.h
               family_metric_list.c family_metric_list.h
               series_list.c series_list.h
               storage.c storage.h
               mql.c mql.h
               "${LIBMDB_SCANNER_C}" "${LIBMDB_SCANNER_H}"
               "${LIBMDB_PARSER_C}" "${LIBMDB_PARSER_H}")

flex_target(LIBMDB_SCANNER "${CMAKE_CURRENT_SOURCE_DIR}/scanner.l"
            "${LIBMDB_SCANNER_C}" DEFINES_FILE "${LIBMDB_SCANNER_H}")
bison_target(LIBMDB_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/parser.y"
             "${LIBMDB_PARSER_C}" DEFINES_FILE "${LIBMDB_PARSER_H}")
add_flex_bison_dependency(LIBMDB_SCANNER LIBMDB_PARSER)

add_library(libmdb STATIC ${LIBMDB_SCANNER_C} "${LIBMDB_PARSER_C}" ${LIBMDB_SRC})
set_target_properties(libmdb PROPERTIES PREFIX "")
set_target_properties(libmdb PROPERTIES POSITION_INDEPENDENT_CODE ON)
# add_dependencies(libmdb libutils)
# add_dependencies(libmdb libmetric)
