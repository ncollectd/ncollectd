set(LIBUTILS_SRC avltree.c avltree.h
                 buf.c buf.h buf_pb.h
                 common.c common.h
                 dtoa.c dtoa.h
                 heap.c heap.h
                 itoa.c itoa.h
                 mount.c mount.h
                 strbuf.c strbuf.h
                 llist.c llist.h
                 complain.c complain.h
                 random.c random.h
                 time.c time.h
                 tail.c tail.h
                 strlist.c strlist.h
                 exclist.c exclist.h
                 htable.c htable.h
                 config.c config.h
                 socket.c socket.h
                 exec.c exec.h)

if(BUILD_SOLARIS)
    set(LIBUTILS_SRC ${LIBUTILS_SRC} kstat.c kstat.h)
endif()

if(NOT HAVE_GETOPT_LONG)
    set(LIBUTILS_SRC ${LIBUTILS_SRC} getopt_long.c getopt_long.h)
endif()

add_library(libutils STATIC ${LIBUTILS_SRC})
set_target_properties(libutils PROPERTIES PREFIX "")
set_target_properties(libutils PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(test_libutils_avltree EXCLUDE_FROM_ALL avltree_test.c)
target_link_libraries(test_libutils_avltree libutils libtest)
add_dependencies(build_tests test_libutils_avltree)
add_test(NAME test_libutils_avltree COMMAND test_libutils_avltree)

add_executable(test_libutils_mount EXCLUDE_FROM_ALL mount_test.c)
target_link_libraries(test_libutils_mount libutils libtest)
add_dependencies(build_tests test_libutils_mount)
add_test(NAME test_libutils_mount COMMAND test_libutils_mount)

add_executable(test_libutils_heap EXCLUDE_FROM_ALL heap_test.c)
target_link_libraries(test_libutils_heap libutils libtest Threads::Threads)
add_dependencies(build_tests test_libutils_heap)
add_test(NAME test_libutils_heap COMMAND test_libutils_heap)

add_executable(test_libutils_time EXCLUDE_FROM_ALL time_test.c)
target_link_libraries(test_libutils_time libutils libtest)
add_dependencies(build_tests test_libutils_time)
add_test(NAME test_libutils_time COMMAND test_libutils_time)

add_executable(test_libutils_strbuf EXCLUDE_FROM_ALL strbuf_test.c)
target_link_libraries(test_libutils_strbuf libutils libtest)
add_dependencies(build_tests test_libutils_strbuf)
add_test(NAME test_libutils_strbuf COMMAND test_libutils_strbuf)

add_executable(test_libutils_common EXCLUDE_FROM_ALL common_test.c)
target_link_libraries(test_libutils_common libutils libtest)
add_dependencies(build_tests test_libutils_common)
add_test(NAME test_libutils_common COMMAND test_libutils_common)

add_executable(test_libutils_itoa EXCLUDE_FROM_ALL itoa_test.c)
target_link_libraries(test_libutils_itoa libutils libtest)
add_dependencies(build_tests test_libutils_itoa)
add_test(NAME test_libutils_itoa COMMAND test_libutils_itoa)

add_executable(test_libutils_dtoa EXCLUDE_FROM_ALL dtoa_test.c)
target_link_libraries(test_libutils_dtoa libutils libtest)
add_dependencies(build_tests test_libutils_dtoa)
add_test(NAME test_libutils_dtoa COMMAND test_libutils_dtoa)
