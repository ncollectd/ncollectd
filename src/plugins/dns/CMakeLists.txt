plugin_option(PLUGIN_DNS "DNS query metrics" ON)

set(BUILD_PLUGIN_DNS 0)

if(PLUGIN_DNS)
    find_package(LibCares)
    if (LIBCARES_FOUND)
        set(BUILD_PLUGIN_DNS 1)
    else()
        set(BUILD_PLUGIN_DNS_REASON "libcares not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_DNS ${BUILD_PLUGIN_DNS} PARENT_SCOPE)

if(BUILD_PLUGIN_DNS)
    set(PLUGIN_DNS_SRC dns.c)
    add_library(dns MODULE ${PLUGIN_DNS_SRC})
    target_link_libraries(dns PRIVATE libmetric libutils libexpr LibCares::LibCares)
    set_target_properties(dns PROPERTIES PREFIX "")
    install(TARGETS dns DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-dns.5 ncollectd-dns.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-dns.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
