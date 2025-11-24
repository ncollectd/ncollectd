cmake_minimum_required(VERSION 3.13)

project(ncollectd C)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

include(GNUInstallDirs)
include(CheckCCompilerFlag)
include(CheckCCompilerFlags)
include(CheckCSourceRuns)
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CheckStructHasMember)
include(CTest)

option(NCOLLECTD_DEBUG "Enable debugging" OFF)
# NCOLLECTD_MNTTAB
#option(NCOLLECTD_GETIFADDRS "Enable feature getifaddrs under Linux" No)
set(ENABLE_ALL_PLUGINS AUTO CACHE STRING "Enable or disable all plugins: AUTO|ON|OFF")
set_property(CACHE ENABLE_ALL_PLUGINS PROPERTY STRINGS AUTO ON OFF)
option(ENABLE_LTO "Check whether the compiler supports Link time optimization (IPO/LTO)" ON)
option(ENABLE_LINK_GC "Enable Linker Garbage Collection" OFF)
option(ENABLE_WERROR "Build with -Werror" ON)

find_package(Git)
if(GIT_EXECUTABLE)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
                    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
                    ERROR_VARIABLE GIT_DESCRIBE_ERROR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT GIT_DESCRIBE_ERROR_CODE)
        execute_process(COMMAND ${GIT_EXECUTABLE} for-each-ref
                            "--format=\"%(creatordate:format:%d %B %Y)\""
                            refs/tags/${GIT_DESCRIBE_VERSION}
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                        OUTPUT_VARIABLE GIT_FOR_EACH_REF_DATE
                        RESULT_VARIABLE GIT_FOR_EACH_REF_ERROR_CODE
                        ERROR_VARIABLE GIT_FOR_EACH_REF_ERROR
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX REPLACE "^v" "" GIT_DESCRIBE_VERSION "${GIT_DESCRIBE_VERSION}")
        string(REGEX REPLACE "-([0-9]+)-g([a-f0-9]+)$" "+\\1.g\\2" GIT_DESCRIBE_VERSION "${GIT_DESCRIBE_VERSION}")
        set(NCOLLECTD_VERSION ${GIT_DESCRIBE_VERSION})
        if(NOT GIT_FOR_EACH_REF_ERROR_CODE AND GIT_FOR_EACH_REF_DATE)
            set(NCOLLECTD_DATE ${GIT_FOR_EACH_REF_DATE})
        else()
            string(TIMESTAMP NCOLLECTD_DATE "%d %B %Y" UTC)
        endif()
    endif()
endif()

if(NOT DEFINED NCOLLECTD_VERSION)
    if(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
        file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" VERSION_STRING)
        string(REPLACE "\\ " " " VERSION_STRING "${VERSION_STRING}")
        string(REPLACE "\\;" ";" VERSION_STRING "${VERSION_STRING}")
        list(GET VERSION_STRING 0 NCOLLECTD_VERSION)
        list(GET VERSION_STRING 1 NCOLLECTD_DATE)
    else()
        set(NCOLLECTD_VERSION "0.0.0+git")
        string(TIMESTAMP NCOLLECTD_DATE "%d %B %Y" UTC)
    endif()
endif()

set(NCOLLECTD_USERAGENT "${CMAKE_PROJECT_NAME}/${NCOLLECTD_VERSION}"
                        CACHE STRING "User Agent for http requests")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(KERNEL_LINUX TRUE)
    set(BUILD_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(KERNEL_FREEBSD TRUE)
    set(BUILD_FREEBSD TRUE)
    set(BUILD_BSD TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly")
    set(KERNEL_DRAGONFLY TRUE)
    set(BUILD_DRAGONFLY TRUE)
    set(BUILD_BSD TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    set(KERNEL_OPENBSD TRUE)
    set(BUILD_OPENBSD TRUE)
    set(BUILD_BSD TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    set(KERNEL_NETBSD TRUE)
    set(BUILD_NETBSD TRUE)
    set(BUILD_BSD TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
    set(KERNEL_AIX TRUE)
    set(BUILD_AIX TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
    set(KERNEL_SOLARIS TRUE)
    set(BUILD_SOLARIS TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(KERNEL_DARWIN TRUE)
    set(BUILD_DARWIN TRUE)
endif()

find_package(BISON 3.0.4 REQUIRED)
find_package(FLEX 2.5.35 REQUIRED)
find_package(GPERF 3.1 REQUIRED)

if(ENABLE_LTO)
    if(NOT DEFINED IPO_SUPPORTED)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR LANGUAGES C)

        if(IPO_SUPPORTED)
            message(STATUS "IPO / LTO: enabled")
            SET(IPO_SUPPORTED 1 CACHE INTERNAL "Result of test for IPO / LTO" FORCE)
        else()
            message(STATUS "IPO / LTO: not supported")
        endif()
    endif()

    if(IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if(ENABLE_LINK_GC)
    message(STATUS "Linker Garbage Collection enabled")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
endif()

string(REPLACE "-Wl,--no-undefined" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

if(BUILD_AIX)
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
    if("$ENV{OBJECT_MODE}" STREQUAL "64")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -maix64 -static-libgcc")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-b64 -static-libgcc -Wl,-bbigtoc")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -Wl,-bbigtoc")
    endif()
endif()

check_c_compiler_flag(-ffile-prefix-map=\"${CMAKE_SOURCE_DIR}=.\" CC_HAVE_FFILE_PREFIX_MAP)
if(CC_HAVE_FFILE_PREFIX_MAP)
    add_definitions(-ffile-prefix-map=\"${CMAKE_SOURCE_DIR}=.\")
else()
    check_c_compiler_flag(-fmacro-prefix-map=\"${CMAKE_SOURCE_DIR}=.\" CC_HAVE_FMACRO_PREFIX_MAP)
    if(CC_HAVE_FMACRO_PREFIX_MAP)
        add_definitions(-fmacro-prefix-map=\"${CMAKE_SOURCE_DIR}=.\")
    endif()
    check_c_compiler_flag(-fdebug-prefix-map=\"${CMAKE_SOURCE_DIR}=.\" CC_HAVE_FDEBUG_PREFIX_MAP)
    if(CC_HAVE_FDEBUG_PREFIX_MAP)
        add_definitions(-fdebug-prefix-map=\"${CMAKE_SOURCE_DIR}=.\")
    endif()
endif()

add_definitions(-O2 -g)

add_definitions(-Wall -Wextra -Wpedantic)

if(ENABLE_WERROR)
    add_definitions(-Werror)
endif()

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
# -Wformat-truncation=2
    check_c_compiler_flags(COMPILER_FLAGS
                           -Wundef             -Wformat-security        -Wnull-dereference
                           -Warray-bounds=2    -Wimplicit-fallthrough=3 -Wlogical-op
                           -Wduplicated-cond   -Wduplicated-branches    -Wformat-signedness
                           -Wshadow            -Wstrict-prototypes      -Wformat=2
                           -Wformat-overflow=2 -Wnull-dereference       -Wtrampolines
                           -Walloca            -Wcast-qual              -Warith-conversion
                           -Wundef             -Wno-missing-field-initializers
                           -Wstrict-overflow=2)
    add_definitions(${COMPILER_FLAGS})
elseif ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    check_c_compiler_flags(COMPILER_FLAGS
                           -Wno-initializer-overrides      -Wno-unused-but-set-variable
                           -Wno-missing-field-initializers -Wno-unknown-warning-option
                           -Wno-gnu-pointer-arith          -Wno-newline-eof
                           -Wno-incompatible-pointer-types-discards-qualifiers
                           -Wno-gnu-statement-expression-from-macro-expansion
                           -Wno-implicit-const-int-float-conversion
                           -Wno-ignored-optimization-argument
                           -Wno-unused-command-line-argument)
    add_definitions(${COMPILER_FLAGS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

find_package(Math REQUIRED)

include(CheckNAN)
check_nan()

include(CheckFP)
check_fp()

include(CheckLargeFiles)
check_large_files(CHECK_LARGEFILES)
if(NEED_LARGEFILE_SOURCE)
    add_definitions(-D_LARGEFILE_SOURCE=1)
endif()
if(NEED_LARGE_FILES)
    add_definitions(-D_LARGE_FILES=1)
endif()
if(NEED_FILE_OFFSET_BITS)
    add_definitions(-D_FILE_OFFSET_BITS=${NEED_FILE_OFFSET_BITS})
endif()

include(CheckCommonDeps)
check_common_deps()

if(BUILD_DARWIN)
    include(CheckDarwinDeps)
    check_darwin_deps()
endif()

include(CheckCommonLibs)
check_common_libs()

add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DPACKAGE_NAME=\"${CMAKE_PROJECT_NAME}\")
add_definitions(-DPACKAGE_VERSION=\"${NCOLLECTD_VERSION}\")
add_definitions(-DDATA_MAX_NAME_LEN=256)

if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
    add_definitions(-DPREFIX=\"\")
else()
    add_definitions(-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\")
endif()

if(BUILD_SOLARIS)
    add_definitions(-D_POSIX_PTHREAD_SEMANTICS)
elseif(BUILD_AIX)
    add_definitions(-D_THREAD_SAFE_ERRNO)
endif()

set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_MODULE_LIBRARY_PREFIX "")

set(CMAKE_VERBOSE_MAKEFILE FALSE)

add_custom_target(build_tests)
add_custom_target(run_tests COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(run_tests build_tests)

configure_file(src/config.h.in src/config.h)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/src")

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LIBDIR}/ncollectd)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/ncollectd)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/run)
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/lib/${CMAKE_PROJECT_NAME})

find_program(MEMORYCHECK_COMMAND valgrind)

set(MEMORYCHECK_COMMAND_OPTIONS "--quiet --tool=memcheck --error-exitcode=1 \
--trace-children=yes --leak-check=full --gen-suppressions=all")

if(EXISTS "${CMAKE_SOURCE_DIR}/src/valgrind.suppress")
    set(MEMORYCHECK_COMMAND_OPTIONS
        "${MEMORYCHECK_COMMAND_OPTIONS} --suppressions=${CMAKE_SOURCE_DIR}/src/valgrind.suppress")
endif()

foreach(_ARCH IN LISTS Linux FreeBSD DragonFly OpenBSD NetBSD AIX Solaris Darwin)
    string(TOUPPER "BUILD_${_ARCH}" _BUILD_ARCH)
    if(${_BUILD_ARCH} AND EXISTS "${CMAKE_SOURCE_DIR}/src/valgrind.${_ARCH}.suppress")
        set(MEMORYCHECK_COMMAND_OPTIONS "${MEMORYCHECK_COMMAND_OPTIONS} \
--suppressions=${CMAKE_SOURCE_DIR}/src/valgrind.${_ARCH}.suppress")
    endif()
endforeach()

set(NCOLLECTD_ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${NCOLLECTD_VERSION})
set(VERSION_FILE "${NCOLLECTD_VERSION}\;${NCOLLECTD_DATE}")
add_custom_target(dist
    COMMAND git archive --prefix=${NCOLLECTD_ARCHIVE_NAME}/
                --add-virtual-file=${NCOLLECTD_ARCHIVE_NAME}/VERSION:"${VERSION_FILE}" HEAD
            | xz -9 > "${CMAKE_BINARY_DIR}/${NCOLLECTD_ARCHIVE_NAME}.tar.xz"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

if(BUILD_AIX)
    set(CMAKE_C_LINK_EXECUTABLE_WITH_EXPORTS "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> \
-Wl,-brtl -Wl,-bexpall <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
endif()

add_subdirectory(src/libutils)
add_subdirectory(src/libmetric)
add_subdirectory(src/libxson)
add_subdirectory(src/libformat)
add_subdirectory(src/libhttp)
add_subdirectory(src/libmdb)
add_subdirectory(src/libconfig)
add_subdirectory(src/libcompress)
add_subdirectory(src/libdbquery)
add_subdirectory(src/libexpr)
add_subdirectory(src/libgraph)
add_subdirectory(src/libquickjs)
add_subdirectory(src/libtest)

add_subdirectory(src/ncollectdmon)
add_subdirectory(src/ncollectdctl)
add_subdirectory(src/ncollectd)

function(plugin_option _PLUGIN_VAR _PLUGIN_HELP _PLUGIN_ENABLED)
    if(ENABLE_ALL_PLUGINS STREQUAL "AUTO")
        if(_PLUGIN_ENABLED)
            set(_ENABLE_PLUGIN ON)
        else()
            set(_ENABLE_PLUGIN OFF)
        endif()
    elseif(ENABLE_ALL_PLUGINS)
        set(_ENABLE_PLUGIN ON)
    else()
        set(_ENABLE_PLUGIN OFF)
    endif()

    option(${_PLUGIN_VAR} ${_PLUGIN_HELP} ${_ENABLE_PLUGIN})
endfunction()

if(BUILD_SOLARIS)
    set(CMAKE_MODULE_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-M,${CMAKE_SOURCE_DIR}/src/plugins.script")
elseif(BUILD_DARWIN)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,dynamic_lookup \
-Wl,-exported_symbol,_module_register")
elseif(BUILD_AIX)
    set(CMAKE_C_CREATE_SHARED_MODULE "<CMAKE_C_COMPILER> <CMAKE_SHARED_MODULE_C_FLAGS> \
-Wl,-brtl -Wl,-G -Wl,-bexport:${CMAKE_SOURCE_DIR}/src/plugins.export \
-Wl,-bnoentry -Wl,-berok <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> \
<CMAKE_SHARED_MODULE_CREATE_C_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
else()
    set(CMAKE_MODULE_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${CMAKE_SOURCE_DIR}/src/plugins.script")
endif()

file(GLOB plugin_list LIST_DIRECTORIES true src/plugins/*)
foreach(dir ${plugin_list})
    if(IS_DIRECTORY ${dir})
        if(EXISTS "${dir}/CMakeLists.txt")
            add_subdirectory(${dir})
            get_filename_component(PLUGIN_NAME ${dir} NAME)
            list(APPEND PLUGINS ${PLUGIN_NAME})
        endif()
    endif()
endforeach()

message("Configuration:")

message("  Build:")
message("    Platform  . . . . . . . ${CMAKE_SYSTEM_NAME}")
message("    Processor . . . . . . . ${CMAKE_SYSTEM_PROCESSOR}")
message("    Compiler vendor . . . . ${CMAKE_C_COMPILER_ID}")
message("    Compiler version  . . . ${CMAKE_C_COMPILER_VERSION}")
message("    CC  . . . . . . . . . . ${CMAKE_C_COMPILER}")
message("    LD  . . . . . . . . . . ${CMAKE_C_LINK_EXECUTABLE}")
message("    AR  . . . . . . . . . . ${CMAKE_AR}")
message("    RANLIB  . . . . . . . . ${CMAKE_RANLIB}")
get_directory_property(COMPILE_DEFINITIONS COMPILE_DEFINITIONS)
string(REPLACE ";" " " COMPILE_DEFINITIONS_STR "${COMPILE_DEFINITIONS}")
message("    CPPFLAGS  . . . . . . . ${COMPILE_DEFINITIONS_STR}")
message("    Make  . . . . . . . . . ${CMAKE_MAKE_PROGRAM}")
message("    BISON . . . . . . . . . ${BISON_EXECUTABLE}")
message("    BISON version . . . . . ${BISON_VERSION}")
message("    FLEX  . . . . . . . . . ${FLEX_EXECUTABLE}")
message("    FLEX version  . . . . . ${FLEX_VERSION}")
message("    GPERF . . . . . . . . . ${GPERF_EXECUTABLE}")
message("    GPERF version . . . . . ${GPERF_VERSION}")

message("  Install:")
message("    PREFIX  . . . . . . . . ${CMAKE_INSTALL_PREFIX}")
message("    SYSCONFDIR  . . . . . . ${CMAKE_INSTALL_FULL_SYSCONFDIR}")
message("    MANDIR  . . . . . . . . ${CMAKE_INSTALL_FULL_MANDIR}")
message("    BINDIR  . . . . . . . . ${CMAKE_INSTALL_FULL_BINDIR}")
message("    SBINDIR . . . . . . . . ${CMAKE_INSTALL_FULL_SBINDIR}")
message("    LIBDIR  . . . . . . . . ${CMAKE_INSTALL_FULL_LIBDIR}")
message("    Debug . . . . . . . . . ${NCOLLECTD_DEBUG}")
message("    Version . . . . . . . . ${NCOLLECTD_VERSION}")
message("    User Agent  . . . . . . ${NCOLLECTD_USERAGENT}")

message("  Plugins:")
foreach(PLUGIN IN LISTS PLUGINS)
    string(TOUPPER "BUILD_PLUGIN_${PLUGIN}" BUILD_PLUGIN)
    string(LENGTH "${PLUGIN}" PLUGIN_LENGTH)
    math(EXPR PLUGIN_SPACES_LENGTH "(20-${PLUGIN_LENGTH})/2")
    set(PLUGIN_SPACES "")
    foreach(_ID RANGE ${PLUGIN_SPACES_LENGTH})
        set(PLUGIN_SPACES " .${PLUGIN_SPACES}")
    endforeach()
    math(EXPR PLUGIN_SPACES_ODD "${PLUGIN_LENGTH}%2")
    if(PLUGIN_SPACES_ODD EQUAL "1")
        string(PREPEND PLUGIN_SPACES " ")
    endif()
    if(${${BUILD_PLUGIN}})
        message("    ${PLUGIN} ${PLUGIN_SPACES} yes")
    else()
        if(${BUILD_PLUGIN}_REASON)
            message("    ${PLUGIN} ${PLUGIN_SPACES} no (${${BUILD_PLUGIN}_REASON})")
        else()
            message("    ${PLUGIN} ${PLUGIN_SPACES} no")
        endif()
    endif()
endforeach()
