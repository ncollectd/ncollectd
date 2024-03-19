plugin_option(PLUGIN_SLURM "SLURM jobs and nodes status" ON)

set(BUILD_PLUGIN_SLURM 0)

if(PLUGIN_SLURM)
    find_package(LibSlurm)
    if (LIBSLURM_FOUND)
        set(BUILD_PLUGIN_SLURM 1)
    else()
        set(BUILD_PLUGIN_SLURM_REASON "libslurm not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_SLURM ${BUILD_PLUGIN_SLURM} PARENT_SCOPE)

if(BUILD_PLUGIN_SLURM)
    set(PLUGIN_SLURM_SRC slurm.c)
    add_library(slurm MODULE ${PLUGIN_SLURM_SRC})
    target_link_libraries(slurm PRIVATE libutils libmetric LibSlurm::LibSlurm)
    set_target_properties(slurm PROPERTIES PREFIX "")
    install(TARGETS slurm DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-slurm.5 ncollectd-slurm.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-slurm.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
