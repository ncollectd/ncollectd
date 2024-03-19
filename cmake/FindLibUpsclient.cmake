include(FindPackageHandleStandardArgs)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(CheckTypeExists)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBUPSCLIENT QUIET libupsclient)

find_path(LIBUPSCLIENT_INCLUDE_DIR NAMES upsclient.h
          HINTS ${PC_LIBUPSCLIENT_INCLUDEDIR} ${PC_LIBUPSCLIENT_INCLUDE_DIRS})
find_library(LIBUPSCLIENT_LIBRARIES NAMES upsclient
             HINTS ${PC_LIBUPSCLIENT_LIBDIR} ${PC_LIBUPSCLIENT_LIBRARY_DIRS})

find_package_handle_standard_args(LibUpsclient DEFAULT_MSG LIBUPSCLIENT_LIBRARIES LIBUPSCLIENT_INCLUDE_DIR)

check_library_exists(upsclient "upscli_init" "${LIBUPSCLIENT_LIBRARIES}" HAVE_UPSCLI_INIT)

check_library_exists(upsclient "upscli_tryconnect" "${LIBUPSCLIENT_LIBRARIES}" HAVE_UPSCLI_TRYCONNECT)

check_type_exists("UPSCONN_t" "time.h;stdint.h;upsclient.h" HAVE_UPSCONN_T)

check_type_exists("UPSCONN" "time.h;stdint.h;upsclient.h" HAVE_UPSCONN)

foreach(PORT_ARG "uint16_t" "int")
    string(TOUPPER "HAVE_PORT_TYPE_${PORT_ARG}" HAVE_PORT_TYPE)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror")
    set(CMAKE_REQUIRED_INCLUDES ${LIBUPSCLIENT_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${LIBUPSCLIENT_LIBRARIES})
    check_c_source_compiles(
"
#include <time.h>
#include <stdint.h>
#include <upsclient.h>
int main(void)
{
    const char *origname = \"ups@localhost:3493\";
    ${PORT_ARG} port=0;
    char *hostname;
    char *upsname;
    int res = upscli_splitname(origname, &upsname, &hostname, &port);
    return(res);
}
" ${HAVE_PORT_TYPE})
    cmake_pop_check_state()
    if(${HAVE_PORT_TYPE})
        set(UPS_PORT_TYPE "${PORT_ARG}")
        break()
    endif()
endforeach()

foreach(SIZE_ARG "size_t" "unsigned int" "int")
    string(TOUPPER "HAVE_UPSCONN_TYPE_${SIZE_ARG}" HAVE_UPSCONN_TYPE)
    string(REPLACE " " "_" HAVE_UPSCONN_TYPE "${HAVE_UPSCONN_TYPE}")

    cmake_push_check_state(RESET)

    if(HAVE_UPSCONN_T)
        set(CMAKE_REQUIRED_DEFINITIONS "-DHAVE_UPSCONN_T")
    elseif(HAVE_UPSCONN)
        set(CMAKE_REQUIRED_DEFINITIONS "-DHAVE_UPSCONN")
    endif()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror")
    set(CMAKE_REQUIRED_INCLUDES ${LIBUPSCLIENT_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${LIBUPSCLIENT_LIBRARIES})

    check_c_source_compiles(
"
#include <time.h>
#include <stdint.h>
#include <upsclient.h>

#if HAVE_UPSCONN_T
typedef UPSCONN_t ncollectd_upsconn_t;
#elif HAVE_UPSCONN
typedef UPSCONN ncollectd_upsconn_t;
#else
#error \"Unable to determine the UPS connection type.\"
#endif
int main(void)
{
    ${SIZE_ARG} query_num=0;
    ${SIZE_ARG} answer_num=0;
    const char * query;
    char** answer;
    ncollectd_upsconn_t ups;
    int res = upscli_list_next(&ups, query_num, &query, &answer_num, &answer);
    return(res);
}
" ${HAVE_UPSCONN_TYPE})
    cmake_pop_check_state()

    if(${HAVE_UPSCONN_TYPE})
        set(UPSCONN_SIZE_TYPE "${SIZE_ARG}")
        break()
    endif()
endforeach()

mark_as_advanced(LIBUPSCLIENT_INCLUDE_DIR LIBUPSCLIENT_LIBRARIES)

if(LIBUPSCLIENT_FOUND AND NOT TARGET LibUpsclient::LibUpsclient)
    set(LIBUPSCLIENT_INCLUDE_DIRS "${LIBUPSCLIENT_INCLUDE_DIR}")
    set(LIBUPSCLIENT_CFLAGS ${PC_LIBUPSCLIENT_CFLAGS_OTHER})

    if(HAVE_UPSCLI_INIT)
        list(APPEND LIBUPSCLIENT_DEFINITIONS HAVE_UPSCLI_INIT)
    endif()
    if(HAVE_UPSCLI_TRYCONNECT)
        list(APPEND LIBUPSCLIENT_DEFINITIONS HAVE_UPSCLI_TRYCONNECT)
    endif()
    if(HAVE_UPSCONN_T)
        list(APPEND LIBUPSCLIENT_DEFINITIONS HAVE_UPSCONN_T)
    elseif(HAVE_UPSCONN)
        list(APPEND LIBUPSCLIENT_DEFINITIONS HAVE_UPSCONN)
    endif()
    if(UPS_PORT_TYPE)
        list(APPEND LIBUPSCLIENT_DEFINITIONS "NUT_PORT_TYPE=${UPS_PORT_TYPE}")
    endif()
    if(UPSCONN_SIZE_TYPE)
        list(APPEND LIBUPSCLIENT_DEFINITIONS "NUT_SIZE_TYPE=${UPSCONN_SIZE_TYPE}")
    endif()

    add_library(LibUpsclient::LibUpsclient INTERFACE IMPORTED)
    set_target_properties(LibUpsclient::LibUpsclient PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBUPSCLIENT_CFLAGS}"
                          INTERFACE_COMPILE_DEFINITIONS "${LIBUPSCLIENT_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBUPSCLIENT_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBUPSCLIENT_LIBRARIES}")
endif()
