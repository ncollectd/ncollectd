plugin_option(PLUGIN_GPU_INTEL "Level-Zero Sysman GPU intel plugin" ON)

set(BUILD_PLUGIN_GPU_INTEL 0)

if(PLUGIN_GPU_INTEL)
    find_package(LibSysman)
    if(LIBSYSMAN_FOUND)
        set(BUILD_PLUGIN_GPU_INTEL 1)
    else()
        set(BUILD_PLUGIN_GPU_INTEL_REASON "sysman not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_GPU_INTEL ${BUILD_PLUGIN_GPU_INTEL} PARENT_SCOPE)

if(BUILD_PLUGIN_GPU_INTEL)
    set(PLUGIN_GPU_INTEL_SRC gpu_intel.c)
    add_library(gpu_intel MODULE ${PLUGIN_GPU_INTEL_SRC})
    target_link_libraries(gpu_intel PRIVATE libmetric libutils LibSysman::LibSysman)
    set_target_properties(gpu_intel PROPERTIES PREFIX "")

#    add_executable(test_plugin_gpu_intel EXCLUDE_FROM_ALL gpu_intel_test.c ${PLUGIN_GPU_INTEL_SRC})
#    target_link_libraries(test_plugin_gpu_intel libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_gpu_intel)
#    add_test(NAME test_plugin_gpu_intel COMMAND test_plugin_gpu_intel WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS gpu_intel DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-gpu_intel.5 ncollectd-gpu_intel.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-gpu_intel.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
