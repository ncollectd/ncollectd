plugin_option(PLUGIN_CEPH "Ceph daemon statistics" ON)

set(BUILD_PLUGIN_CEPH 0)

if(PLUGIN_CEPH)
    set(BUILD_PLUGIN_CEPH 1)
endif()

set(BUILD_PLUGIN_CEPH ${BUILD_PLUGIN_CEPH} PARENT_SCOPE)

if(BUILD_PLUGIN_CEPH)
    set(PLUGIN_CEPH_SRC ceph.c)
    add_library(ceph MODULE ${PLUGIN_CEPH_SRC})
    target_link_libraries(ceph PRIVATE libutils libmetric libxson)
    set_target_properties(ceph PROPERTIES PREFIX "")

#    add_executable(test_plugin_ceph EXCLUDE_FROM_ALL ceph_test.c ${PLUGIN_CEPH_SRC})
#    target_link_libraries(test_plugin_ceph libtest libutils libmetric -lm)
#    add_dependencies(build_tests test_plugin_ceph)
#    add_test(NAME test_plugin_ceph COMMAND test_plugin_ceph WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ceph DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ceph.5 ncollectd-ceph.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ceph.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
