set(NCOLLECTDMON_SRC ncollectdmon.c)

add_executable(ncollectdmon ${NCOLLECTDMON_SRC})

install(TARGETS ncollectdmon DESTINATION ${CMAKE_INSTALL_SBINDIR})
configure_file(ncollectdmon.1 ncollectdmon.1 @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectdmon.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
