plugin_option(PLUGIN_CPU "CPU usage statistics" ON)

include(CheckIncludeFile)

set(BUILD_PLUGIN_CPU 0)

if(PLUGIN_CPU)
    if(BUILD_LINUX)
        set(BUILD_PLUGIN_CPU 1)
    elseif(BUILD_AIX)
        find_package(LibPerfstat)
        if (LIBPERFSTAT_FOUND)
            set(BUILD_PLUGIN_CPU 1)
        else()
            set(BUILD_PLUGIN_CPU_REASON "perfstat not found" PARENT_SCOPE)
        endif()
    elseif(BUILD_BSD)
        if(HAVE_SYSCTLBYNAME)
            check_c_source_runs("
#include <sys/types.h>
#include <sys/sysctl.h>
int main(void)
{
    long cpuinfo[256][5];
    size_t cpuinfo_size = sizeof(cpuinfo);
    if (sysctlbyname(\"kern.cp_times\", &cpuinfo, &cpuinfo_size, (void *)0, 0) < 0)
        return 1;
    return 0;
}
" HAVE_SYSCTL_KERN_CP_TIMES)
            check_c_source_runs("
#include <sys/types.h>
#include <sys/sysctl.h>
int main(void)
{
    long cpuinfo[5];
    size_t cpuinfo_size = sizeof(cpuinfo);
    if (sysctlbyname(\"kern.cp_time\", &cpuinfo, &cpuinfo_size, (void*)0, 0) < 0)
        return 1;
    return 0;
}
" HAVE_SYSCTL_KERN_CP_TIME)
        endif()
        check_include_file(sys/dkstat.h HAVE_SYS_DKSTAT_H)
        set(BUILD_PLUGIN_CPU 1)
    elseif(BUILD_DARWIN)
        set(BUILD_PLUGIN_CPU 1)
    elseif(BUILD_SOLARIS)
        find_package(LibKstat)
        if (LIBKSTAT_FOUND)
            set(BUILD_PLUGIN_CPU 1)
        else()
            set(BUILD_PLUGIN_CPU_REASON "kstat not found" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_CPU_REASON "unsupported system" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_CPU ${BUILD_PLUGIN_CPU} PARENT_SCOPE)

if(BUILD_PLUGIN_CPU)
    set(PLUGIN_CPU_SRC cpu.c cpu.h)
    set(PLUGIN_CPU_LIBS libutils libmetric)

    if(BUILD_LINUX)
        list(APPEND PLUGIN_CPU_SRC cpu_linux.c)
    elseif(BUILD_AIX)
        list(APPEND PLUGIN_CPU_SRC cpu_aix.c)
        list(APPEND PLUGIN_CPU_LIBS LibPerfstat::LibPerfstat)
    elseif(BUILD_BSD)
        if(HAVE_SYS_DKSTAT_H)
            list(APPEND PLUGIN_CPU_DEFINITIONS HAVE_SYS_DKSTAT_H)
        endif()
        if(HAVE_SYSCTL_KERN_CP_TIME)
            list(APPEND PLUGIN_CPU_DEFINITIONS HAVE_SYSCTL_KERN_CP_TIME)
        endif()
        if(HAVE_SYSCTL_KERN_CP_TIMES)
            list(APPEND PLUGIN_CPU_DEFINITIONS HAVE_SYSCTL_KERN_CP_TIMES)
        endif()
        list(APPEND PLUGIN_CPU_SRC cpu_bsd.c)
    elseif(BUILD_DARWIN)
        list(APPEND PLUGIN_CPU_SRC cpu_darwin.c)
    elseif(BUILD_SOLARIS)
        list(APPEND PLUGIN_CPU_SRC cpu_solaris.c)
        list(APPEND PLUGIN_CPU_LIBS LibKstat::LibKstat)
    endif()

    add_library(cpu MODULE ${PLUGIN_CPU_SRC})
    target_link_libraries(cpu PRIVATE ${PLUGIN_CPU_LIBS})
    target_compile_definitions(cpu PUBLIC ${PLUGIN_CPU_DEFINITIONS})
    set_target_properties(cpu PROPERTIES PREFIX "")

    if(BUILD_LINUX)
        add_executable(test_plugin_cpu EXCLUDE_FROM_ALL cpu_linux_test.c ${PLUGIN_CPU_SRC})
        target_link_libraries(test_plugin_cpu libtest libutils libmetric m)
        add_dependencies(build_tests test_plugin_cpu)
        add_test(NAME test_plugin_cpu COMMAND test_plugin_cpu WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()

    install(TARGETS cpu DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-cpu.5 ncollectd-cpu.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-cpu.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
