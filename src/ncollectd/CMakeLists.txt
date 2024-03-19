set(NCOLLECTD_SRC ../plugin.h
                  ../ncollectd.h
                  ../log.h
                  cmd.c cmd.h
                  ncollectd.c
                  configfile.c configfile.h
                  filter.c filter.h
                  globals.c globals.h
                  queue.c queue.h
                  plugin.c plugin_internal.h
                  plugin_match.c plugin_match.h
                  plugin_read.c
                  plugin_write.c
                  plugin_notify.c
                  httpd.c httpd.h)

add_executable(ncollectd ${NCOLLECTD_SRC})

set(NCOLLECTD_LIBS libhttp libmetric libutils libmdb libformat libxson libutils libconfig)
if(BUILD_LINUX)
    find_package(LibCap)
    if(LIBCAP_FOUND)
        list(APPEND NCOLLECTD_LIBS LibCap::LibCap)
    endif()
endif()

find_package(LibAtomic)
if(LIBATOMIC_FOUND)
    list(APPEND NCOLLECTD_LIBS LibAtomic::LibAtomic)
endif()

list(APPEND NCOLLECTD_LIBS Math::Math Threads::Threads ${CMAKE_DL_LIBS})

target_link_libraries(ncollectd ${NCOLLECTD_LIBS})
set_target_properties(ncollectd PROPERTIES ENABLE_EXPORTS ON)

install(TARGETS ncollectd DESTINATION ${CMAKE_INSTALL_SBINDIR})
configure_file(ncollectd.1 ncollectd.1 @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
configure_file(ncollectd.conf.5 ncollectd.conf.5 @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd.conf.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)

configure_file(ncollectd.conf.in ncollectd.conf @ONLY)
install(CODE "
    if(NOT EXISTS \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/ncollectd.conf\")
        file(INSTALL \"${CMAKE_BINARY_DIR}/src/ncollectd/ncollectd.conf\"
             DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}\")
    endif()
")

# if BUILD_FEATURE_DAEMON
# ncollectd_CPPFLAGS += -DPIDFILE='"${localstatedir}/run/${PACKAGE_NAME}.pid"'
# endif
