plugin_option(PLUGIN_GPU_NVIDIA "NVIDIA GPU plugin" ON)

set(BUILD_PLUGIN_GPU_NVIDIA 0)

if(PLUGIN_GPU_NVIDIA)
    find_package(LibCuda)
    if(LIBCUDA_FOUND)
        set(BUILD_PLUGIN_GPU_NVIDIA 1)
    else()
        set(BUILD_PLUGIN_GPU_NVIDIA_REASON "cuda not found" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_GPU_NVIDIA ${BUILD_PLUGIN_GPU_NVIDIA} PARENT_SCOPE)

if(BUILD_PLUGIN_GPU_NVIDIA)
    set(PLUGIN_GPU_NVIDIA_SRC gpu_nvidia.c)
    add_library(gpu_nvidia MODULE ${PLUGIN_GPU_NVIDIA_SRC})
    target_link_libraries(gpu_nvidia PRIVATE libmetric libutils LibCuda::LibCuda)
    set_target_properties(gpu_nvidia PROPERTIES PREFIX "")

#    add_executable(test_plugin_gpu_nvidia EXCLUDE_FROM_ALL gpu_nvidia_test.c ${PLUGIN_GPU_NVIDIA_SRC})
#    target_link_libraries(test_plugin_gpu_nvidia libtest libmetric libutils -lm)
#    add_dependencies(build_tests test_plugin_gpu_nvidia)
#    add_test(NAME test_plugin_gpu_nvidia COMMAND test_plugin_gpu_nvidia WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS gpu_nvidia DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-gpu_nvidia.5 ncollectd-gpu_nvidia.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-gpu_nvidia.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
