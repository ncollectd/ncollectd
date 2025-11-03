plugin_option(PLUGIN_SCRAPER "Scrape metrics" ON)

set(BUILD_PLUGIN_SCRAPER 0)

if(PLUGIN_SCRAPER)
    find_package(LibCurl)
    if(LIBCURL_FOUND)
        set(BUILD_PLUGIN_SCRAPER 1)
    else()
        set(BUILD_PLUGIN_SCRAPER_REASON "libcurl not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SCRAPER ${BUILD_PLUGIN_SCRAPER} PARENT_SCOPE)

if(BUILD_PLUGIN_SCRAPER)
    set(PLUGIN_SCRAPER_SRC scraper.c curl_stats.c curl_stats.h)
    add_library(scraper MODULE ${PLUGIN_SCRAPER_SRC})
    target_link_libraries(scraper PRIVATE libmetric libutils LibCurl::LibCurl)
    set_target_properties(scraper PROPERTIES PREFIX "")
    install(TARGETS scraper DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-scraper.5 ncollectd-scraper.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-scraper.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
