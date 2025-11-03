plugin_option(PLUGIN_PYTHON "Embed a Python interpreter" ON)

set(BUILD_PLUGIN_PYTHON 0)

if(PLUGIN_PYTHON)
    find_package(LibPython)
    if(LIBPYTHON_FOUND)
        set(BUILD_PLUGIN_PYTHON 1)
    else()
        set(BUILD_PLUGIN_PYTHON_REASON "libpython not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_PYTHON ${BUILD_PLUGIN_PYTHON} PARENT_SCOPE)

if(BUILD_PLUGIN_PYTHON)
    set(PLUGIN_PYTHON_SRC python.c pyutils.c pyconfig.c pymetric.c pynotification.c
                          pymetricfamily.c cpython.h)
    add_library(python MODULE ${PLUGIN_PYTHON_SRC})
    target_link_libraries(python PRIVATE libmetric libutils LibPython::LibPython)
    set_target_properties(python PROPERTIES PREFIX "")

#    add_executable(test_plugin_python EXCLUDE_FROM_ALL python_test.c ${PLUGIN_PYTHON_SRC})
#    target_link_libraries(test_plugin_python libtest libmetric libutils m)
#    add_dependencies(build_tests test_plugin_python)
#    add_test(NAME test_plugin_python COMMAND test_plugin_python WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS python DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-python.5 ncollectd-python.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-python.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
