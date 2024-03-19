plugin_option(PLUGIN_NOTIFY_EMAIL "Email notifier" ON)

set(BUILD_PLUGIN_NOTIFY_EMAIL 0)

if(PLUGIN_NOTIFY_EMAIL)
    find_package(LibEsmtp)
    if (LIBESMTP_FOUND)
        set(BUILD_PLUGIN_NOTIFY_EMAIL 1)
    else()
        set(BUILD_PLUGIN_NOTIFY_EMAIL_REASON "libesmtp not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_NOTIFY_EMAIL ${BUILD_PLUGIN_NOTIFY_EMAIL} PARENT_SCOPE)

if(BUILD_PLUGIN_NOTIFY_EMAIL)
    set(PLUGIN_NOTIFY_EMAIL_SRC notify_email.c)
    add_library(notify_email MODULE ${PLUGIN_NOTIFY_EMAIL_SRC})
    target_link_libraries(notify_email PRIVATE libutils libmetric LibEsmtp::LibEsmtp)
    set_target_properties(notify_email PROPERTIES PREFIX "")
    install(TARGETS notify_email DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-notify_email.5 ncollectd-notify_email.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-notify_email.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
