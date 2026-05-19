plugin_option(PLUGIN_EBPF "eBPF statistics" ON)
include(CheckIncludeFile)

set(BUILD_PLUGIN_EBPF 0)

if(PLUGIN_EBPF)
    if(BUILD_LINUX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm.*|mips.*|ppc.*|ppc64.*|loongarch64|x86_64|riscv32|riscv64")
            check_include_file(linux/bpf.h HAVE_LINUX_BPF_H)
            if(HAVE_LINUX_BPF_H)
                check_include_file(linux/perf_event.h HAVE_LINUX_PERF_EVENT_H)
                if (HAVE_LINUX_PERF_EVENT_H)
                    check_include_file(linux/version.h HAVE_LINUX_VERSION_H)
                    if (HAVE_LINUX_VERSION_H)
                        set(BUILD_PLUGIN_EBPF 1)
                    else()
                        set(BUILD_PLUGIN_EBPF_REASON "linux/version.h not found" PARENT_SCOPE)
                    endif()
                else()
                    set(BUILD_PLUGIN_EBPF_REASON "linux/perf_event.h not found" PARENT_SCOPE)
                endif()
            else()
                set(BUILD_PLUGIN_EBPF_REASON "linux/bpf.h not found" PARENT_SCOPE)
            endif()
        else()
            set(BUILD_PLUGIN_EBPF_REASON "Unsupported architecture" PARENT_SCOPE)
        endif()
    else()
        set(BUILD_PLUGIN_EBPF_REASON "Linux only" PARENT_SCOPE)
    endif()
endif()

set(BUILD_PLUGIN_EBPF ${BUILD_PLUGIN_EBPF} PARENT_SCOPE)

if(BUILD_PLUGIN_EBPF)
    set(EBPF_LEXER_C "${CMAKE_CURRENT_BINARY_DIR}/lexer.c")
    set(EBPF_LEXER_H "${CMAKE_CURRENT_BINARY_DIR}/lexer.h")
    set(EBPF_GRAMMAR_C "${CMAKE_CURRENT_BINARY_DIR}/grammar.c")
    set(EBPF_GRAMMAR_H "${CMAKE_CURRENT_BINARY_DIR}/grammar.h")

    flex_target(EBPF_LEXERNER "${CMAKE_CURRENT_SOURCE_DIR}/lexer.l"
                "${EBPF_LEXER_C}" DEFINES_FILE "${EBPF_LEXER_H}")
    bison_target(EBPF_PARSER "${CMAKE_CURRENT_SOURCE_DIR}/grammar.y"
                 "${EBPF_GRAMMAR_C}" DEFINES_FILE "${EBPF_GRAMMAR_H}")
    add_flex_bison_dependency(EBPF_LEXERNER EBPF_PARSER)

    set(PLUGIN_EBPF_SRC ebpf.c
                        libply.c ply.h internal.h utils.h sym.c sym.h node.c node.h ir.c ir.h
                        func.c func.h type.c type.h compile.c provider.c provider.h built-in.c
                        built-in.h math.c memory.c print.c buffer.c buffer.h proc.c aggregation.c
                        flow.c special.c tracepoint.c xprobe.c xprobe.h interval.c profile.c
                        kprobe.c kprobe.h kretprobe.c syscall.c syscall.h kallsyms.c kallsyms.h
                        perf_event.c perf_event.h printxf.c printxf.h utils.c arch.h
                        "${EBPF_LEXER_C}" "${EBPF_LEXER_H}"
                        "${EBPF_GRAMMAR_C}" "${EBPF_GRAMMAR_H}")

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        list(APPEND PLUGIN_EBPF_SRC aarch64.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm.*")
        list(APPEND PLUGIN_EBPF_SRC arm.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips.*")
        list(APPEND PLUGIN_EBPF_SRC mips.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc.*")
        list(APPEND PLUGIN_EBPF_SRC powerpc.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64.*")
        list(APPEND PLUGIN_EBPF_SRC powerpc64.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv32")
        list(APPEND PLUGIN_EBPF_SRC riscv32.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
        list(APPEND PLUGIN_EBPF_SRC riscv64.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        list(APPEND PLUGIN_EBPF_SRC x86_64.c)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "loongarch64")
        list(APPEND PLUGIN_EBPF_SRC loongarch64.c)
    endif()

    add_library(ebpf MODULE ${PLUGIN_EBPF_SRC})
    target_link_libraries(ebpf PRIVATE libutils libmetric)
    set_target_properties(ebpf PROPERTIES PREFIX "")

#    add_executable(test_plugin_ebpf EXCLUDE_FROM_ALL ebpf_test.c ${PLUGIN_EBPF_SRC})
#    target_link_libraries(test_plugin_ebpf libtest libutils libmetric m)
#    add_dependencies(build_tests test_plugin_ebpf)
#    add_test(NAME test_plugin_ebpf COMMAND test_plugin_ebpf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    install(TARGETS ebpf DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd/)
    configure_file(ncollectd-ebpf.5 ncollectd-ebpf.5 @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ncollectd-ebpf.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
endif()
