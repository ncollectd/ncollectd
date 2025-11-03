plugin_option(PLUGIN_MATCH_XPATH "Match with XPATH" ON)

set(BUILD_PLUGIN_MATCH_XPATH 0)

if(PLUGIN_MATCH_XPATH)
    find_package(LibXml2)
    if (LIBXML2_FOUND)
        set(BUILD_PLUGIN_MATCH_XPATH 1)
    else()
        set(BUILD_PLUGIN_MATCH_XPATH_REASON "libxml2 not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_MATCH_XPATH ${BUILD_PLUGIN_MATCH_XPATH} PARENT_SCOPE)

if(BUILD_PLUGIN_MATCH_XPATH)
    set(PLUGIN_MATCH_XPATH_SRC match_xpath.c)
    add_library(match_xpath MODULE ${PLUGIN_MATCH_XPATH_SRC})
    target_link_libraries(match_xpath PRIVATE libmetric libutils LibXml2::LibXml2)
    set_target_properties(match_xpath PROPERTIES PREFIX "")
    install(TARGETS match_xpath DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-match_xpath.5 ncollectd-match_xpath.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-match_xpath.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
