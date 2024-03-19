set(NCOLLECTDCTL_SRC ncollectdctl.c client.c client.h)
add_executable(ncollectdctl ${NCOLLECTDCTL_SRC})
target_link_libraries(ncollectdctl libmdb libformat libxson libhttp libutils libmetric -lm)

install(TARGETS ncollectdctl DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(ncollectdctl.1 ncollectdctl.1 @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectdctl.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
