plugin_option(PLUGIN_IRQ "IRQ statistics" ON)

set(BUILD_PLUGIN_IRQ 0)

if(PLUGIN_IRQ)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_IRQ 1)
    elseif(BUILD_NETBSD)
        set(BUILD_PLUGIN_IRQ 1)
    else()
        set(BUILD_PLUGIN_IRQ_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_IRQ ${BUILD_PLUGIN_IRQ} PARENT_SCOPE)

if(BUILD_PLUGIN_IRQ)
    set(PLUGIN_IRQ_SRC irq.c)
    add_library(irq MODULE ${PLUGIN_IRQ_SRC})
    target_link_libraries(irq PRIVATE libutils libmetric)
    set_target_properties(irq PROPERTIES PREFIX "")

    if(BUILD_LINUX)
        add_executable(test_plugin_irq EXCLUDE_FROM_ALL irq_linux_test.c ${PLUGIN_IRQ_SRC})
        target_link_libraries(test_plugin_irq libtest libutils libmetric -lm)
        add_dependencies(build_tests test_plugin_irq)
        add_test(NAME test_plugin_irq COMMAND test_plugin_irq WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()

    install(TARGETS irq DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-irq.5 ncollectd-irq.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-irq.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
