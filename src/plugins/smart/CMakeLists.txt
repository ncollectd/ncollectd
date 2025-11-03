plugin_option(PLUGIN_SMART "SMART statistics" ON)

set(BUILD_PLUGIN_SMART 0)

if(PLUGIN_SMART)
    find_package(LibAtasmart)
    find_package(LibUdev)
    if (LIBATASMART_FOUND)
        if (LIBUDEV_FOUND)
            set(BUILD_PLUGIN_SMART 1)
        else()
            set(BUILD_PLUGIN_SMART_REASON "libudev not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_SMART_REASON "libatasmart not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SMART ${BUILD_PLUGIN_SMART} PARENT_SCOPE)

if(BUILD_PLUGIN_SMART)
    set(PLUGIN_SMART_SRC
        smart.c
        smart_fams.h
        nvme.h
        intel-nvme.h)
    add_library(smart MODULE ${PLUGIN_SMART_SRC})
    target_link_libraries(smart PRIVATE libmetric libutils LibAtasmart::LibAtasmart LibUdev::LibUdev)
    set_target_properties(smart PROPERTIES PREFIX "")
    install(TARGETS smart DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-smart.5 ncollectd-smart.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-smart.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
