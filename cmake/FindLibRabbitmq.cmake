include(FindPackageHandleStandardArgs)
include(CheckStructHasMember)
include(CheckSymbolExists)

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBRABBITMQ QUIET librabbitmq)

find_path(LIBRABBITMQ_INCLUDE_SUBDIRS NAMES rabbitmq-c/amqp.h
          HINTS ${PC_LIBRABBITMQ_INCLUDEDIR} ${PC_LIBRABBITMQ_INCLUDE_DIRS})
  if(NOT LIBRABBITMQ_INCLUDE_SUBDIRS)
    find_path(LIBRABBITMQ_INCLUDE_DIRS NAMES amqp.h
              HINTS ${PC_LIBRABBITMQ_INCLUDEDIR} ${PC_LIBRABBITMQ_INCLUDE_DIRS})
else()
    set(LIBRABBITMQ_INCLUDE_DIRS ${LIBRABBITMQ_INCLUDE_SUBDIRS})
    set(HAVE_RABBITMQ_C_INCLUDE_PATH ON)
    set(LIBRABBITMQ_HAVE_RABBITMQ_C_INCLUDE_PATH ON CACHE BOOL "Have rabbitmq-c/* includes")
endif()

find_library(LIBRABBITMQ_LIBRARIES NAMES rabbitmq
             HINTS ${PC_LIBRABBITMQ_LIBDIR} ${PC_LIBRABBITMQ_LIBRARY_DIRS})

if(HAVE_RABBITMQ_C_INCLUDE_PATH)
    check_struct_has_member(amqp_rpc_reply_t library_errno
                            "stdlib.h;stdio.h;stdint.h;inttypes.h;rabbitmq-c/amqp.h"
                            HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO LANGUAGE C)
else()
    check_struct_has_member(amqp_rpc_reply_t library_errno
                            "stdlib.h;stdio.h;stdint.h;inttypes.h;amqp.h"
                            HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO LANGUAGE C)
endif()
if(HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO)
    set(LIBRABBITMQ_HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO ON CACHE BOOL "Have amqp_rpc_reply.library_errno")
endif()

#    check_library_exists(rabbitmq "amqp_basic_publish" "${LIBRABBITMQ_LIBRARIES}" AMQP_BASIC_PUBLISH)

if(HAVE_RABBITMQ_C_INCLUDE_PATH)
    find_path(HAVE_AMQP_TCP_SOCKET_H NAMES rabbitmq-c/tcp_socket.h
              HINTS ${PC_LIBRABBITMQ_INCLUDEDIR} ${PC_LIBRABBITMQ_INCLUDE_DIRS})
else()
    find_path(HAVE_AMQP_TCP_SOCKET_H NAMES amqp_tcp_socket.h
              HINTS ${PC_LIBRABBITMQ_INCLUDEDIR} ${PC_LIBRABBITMQ_INCLUDE_DIRS})
endif()

if(HAVE_AMQP_TCP_SOCKET_H)
    set(LIBRABBITMQ_HAVE_AMQP_TCP_SOCKET_H  ON CACHE BOOL "Have rabbitmq-c/tcp_socket.h or amqp_tcp_socket.h")
endif()

if(LIBRABBITMQ_HAVE_RABBITMQ_C_INCLUDE_PATH)
    find_path(HAVE_AMQP_SOCKET_H NAMES rabbitmq-c/socket.h
              HINTS ${PC_LIBRABBITMQ_INCLUDEDIR} ${PC_LIBRABBITMQ_INCLUDE_DIRS})
else()
    find_path(HAVE_AMQP_SOCKET_H NAMES amqp_socket.h
              HINTS ${PC_LIBRABBITMQ_INCLUDEDIR} ${PC_LIBRABBITMQ_INCLUDE_DIRS})
endif()

if(HAVE_AMPQ_SOCKET_H)
    set(LIBRABBITMQ_HAVE_AMQP_SOCKET_H ON CACHE BOOL "Have rabbitmq-c/socket.h or amqp_socket.h")
endif()

check_library_exists(rabbitmq "amqp_tcp_socket_new" "${LIBRABBITMQ_LIBRARIES}" HAVE_AMQP_TCP_SOCKET)
if(HAVE_AMQP_TCP_SOCKET)
    set(LIBRABBITMQ_HAVE_AMQP_TCP_SOCKET ON CACHE BOOL "Have amqp_tcp_socket_new")
endif()

if(HAVE_RABBITMQ_C_INCLUDE_PATH)
    set(AMQP_INCLUDES "rabbitmq-c/amqp.h")
    if(HAVE_AMQP_TCP_SOCKET_H)
        set(AMQP_INCLUDES "${AMQP_INCLUDES};rabbitmq-c/tcp_socket.h")
    endif()
    if(HAVE_AMQP_SOCKET_H)
        set(AMQP_INCLUDES "${AMQP_INCLUDES};rabbitmq-c/socket.h")
    endif()
else()
    set(AMQP_INCLUDES "amqp.h")
    if(HAVE_AMQP_TCP_SOCKET_H)
        set(AMQP_INCLUDES "${AMQP_INCLUDES};amqp_tcp_socket.h")
    endif()
    if(HAVE_AMQP_SOCKET_H)
        set(AMQP_INCLUDES "${AMQP_INCLUDES};amqp_socket.h")
    endif()
endif()
check_symbol_exists(amqp_socket_close "${AMQP_INCLUDES}" HAVE_DECL_AMQP_SOCKET_CLOSE)
if(HAVE_DECL_AMQP_SOCKET_CLOSE)
    set(LIBRABBITMQ_HAVE_DECL_AMQP_SOCKET_CLOSE ON CACHE BOOL "Have amqp_socket_close")
endif()

find_package_handle_standard_args(LibRabbitmq DEFAULT_MSG LIBRABBITMQ_LIBRARIES LIBRABBITMQ_INCLUDE_DIRS)

mark_as_advanced(LIBRABBITMQ_INCLUDE_DIRS LIBRABBITMQ_LIBRARIES
                 LIBRABBITMQ_HAVE_RABBITMQ_C_INCLUDE_PATH
                 LIBRABBITMQ_HAVE_AMQP_RPC_REPLY_T_LIBRARY_ERRNO
                 LIBRABBITMQ_HAVE_AMQP_TCP_SOCKET_H
                 LIBRABBITMQ_HAVE_AMQP_SOCKET_H
                 LIBRABBITMQ_HAVE_AMQP_TCP_SOCKET
                 LIBRABBITMQ_HAVE_DECL_AMQP_SOCKET_CLOSE)

if(LIBRABBITMQ_FOUND AND NOT TARGET LibRabbitmq::LibRabbitmq)
    set(LIBRABBITMQ_DEFINITIONS ${PC_LIBRABBITMQ_CFLAGS_OTHER})
    add_library(LibRabbitmq::LibRabbitmq INTERFACE IMPORTED)
    set_target_properties(LibRabbitmq::LibRabbitmq PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${LIBRABBITMQ_DEFINITIONS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBRABBITMQ_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBRABBITMQ_LIBRARIES}")
endif()
