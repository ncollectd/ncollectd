gperf_generate(lookup.gperf "${CMAKE_CURRENT_BINARY_DIR}/lookup.h" LOOKUP_H)

set(LIBHTTP_SRC common.c
                header.c header.h
                parser.c parser.h
                http.c http.h
                httpd.c httpd.h
                "${LOOKUP_H}")

add_library(libhttp STATIC ${LIBHTTP_SRC})
set_target_properties(libhttp PROPERTIES PREFIX "")
set_target_properties(libhttp PROPERTIES POSITION_INDEPENDENT_CODE ON)
# add_dependencies(libhttp libutils)
