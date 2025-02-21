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

add_executable(test_libhttp EXCLUDE_FROM_ALL http_test.c)
target_link_libraries(test_libhttp libhttp libutils libtest -lpthread)
add_dependencies(build_tests test_libhttp)
add_test(NAME test_libhttp COMMAND test_libhttp)
