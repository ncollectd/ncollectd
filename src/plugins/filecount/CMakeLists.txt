plugin_option(PLUGIN_FILECOUNT "Count files in directories" ON)

include(CheckStructHasMember)

set(BUILD_PLUGIN_FILECOUNT 0)

if(PLUGIN_FILECOUNT)
    set(BUILD_PLUGIN_FILECOUNT 1)
endif()

set(BUILD_PLUGIN_FILECOUNT ${BUILD_PLUGIN_FILECOUNT} PARENT_SCOPE)

if(BUILD_PLUGIN_FILECOUNT)
    set(PLUGIN_FILECOUNT_SRC filecount.c)
    add_library(filecount MODULE ${PLUGIN_FILECOUNT_SRC})
    target_link_libraries(filecount PRIVATE libmetric libutils libexpr)
    set_target_properties(filecount PROPERTIES PREFIX "")

#    add_executable(test_plugin_filecount EXCLUDE_FROM_ALL filecount_test.c ${PLUGIN_FILECOUNT_SRC})
#    target_link_libraries(test_plugin_filecount libtest libmetric libutils libexpr -lm)
#    add_dependencies(build_tests test_plugin_filecount)
#    add_test(NAME test_plugin_filecount COMMAND test_plugin_filecount WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS filecount DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-filecount.5 ncollectd-filecount.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-filecount.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
