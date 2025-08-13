# SPDX-License-Identifier: GPL-2.1-or-later
# SPDX-FileCopyrightText: Copyright 2009- The GROMACS Authors and the project initiators Erik Lindahl, Berk Hess and David van der Spoel.

include(CheckTypeSize)

macro(check_large_files VARIABLE)
    if(NOT DEFINED ${VARIABLE})
        # On most platforms it is probably overkill to first test the flags for 64-bit off_t,
        # and then separately fseeko. However, in the future we might have 128-bit filesystems
        # (ZFS), so it might be dangerous to indiscriminately set e.g. _FILE_OFFSET_BITS=64.

        message(STATUS "Checking for 64-bit off_t")

        # First check without any special flags
        try_compile(FILE64_OK "${CMAKE_BINARY_DIR}"
                    "${CMAKE_SOURCE_DIR}/cmake/TestFileOffsetBits.c")
        if(FILE64_OK)
            message(STATUS "Checking for 64-bit off_t - present")
        endif()

        if(NOT FILE64_OK)
            # Test with _FILE_OFFSET_BITS=64
            try_compile(FILE64_OK "${CMAKE_BINARY_DIR}"
                        "${CMAKE_SOURCE_DIR}/cmake/TestFileOffsetBits.c"
                        COMPILE_DEFINITIONS "-D_FILE_OFFSET_BITS=64" )
            if(FILE64_OK)
                message(STATUS "Checking for 64-bit off_t - present with _FILE_OFFSET_BITS=64")
                set(NEED_FILE_OFFSET_BITS 64 CACHE INTERNAL "64-bit off_t requires _FILE_OFFSET_BITS=64")
            endif()
        endif()

        if(NOT FILE64_OK)
            # Test with _LARGE_FILES
            try_compile(FILE64_OK "${CMAKE_BINARY_DIR}"
                        "${CMAKE_SOURCE_DIR}/cmake/TestFileOffsetBits.c"
                        COMPILE_DEFINITIONS "-D_LARGE_FILES" )
            if(FILE64_OK)
                message(STATUS "Checking for 64-bit off_t - present with _LARGE_FILES")
                set(NEED_LARGE_FILES 1 CACHE INTERNAL "64-bit off_t requires _LARGE_FILES")
            endif()
        endif()

        if(NOT FILE64_OK)
            # Test with _LARGEFILE_SOURCE
            try_compile(FILE64_OK "${CMAKE_BINARY_DIR}"
                        "${CMAKE_SOURCE_DIR}/cmake/TestFileOffsetBits.c"
                        COMPILE_DEFINITIONS "-D_LARGEFILE_SOURCE" )
            if(FILE64_OK)
                message(STATUS "Checking for 64-bit off_t - present with _LARGEFILE_SOURCE")
                set(NEED_LARGEFILE_SOURCE 1 CACHE INTERNAL "64-bit off_t requires _LARGEFILE_SOURCE")
            endif()
        endif()

        if(NOT FILE64_OK)
            message(STATUS "Checking for 64-bit off_t - not present")
        else()
            # 64-bit off_t found. Now check that ftello/fseeko is available.

            # Set the flags we might have determined to be required above
            configure_file("${CMAKE_SOURCE_DIR}/cmake/TestLargeFiles.c.cmakein"
                           "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TestLargeFiles.c")

            message(STATUS "Checking for fseeko/ftello")
            # Test if ftello/fseeko are available
            try_compile(FSEEKO_COMPILE_OK "${CMAKE_BINARY_DIR}"
                        "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TestLargeFiles.c")
            if(FSEEKO_COMPILE_OK)
                message(STATUS "Checking for fseeko/ftello - present")
            endif()

            if(NOT FSEEKO_COMPILE_OK)
                # glibc 2.2 needs _LARGEFILE_SOURCE for fseeko (but not 64-bit off_t...)
                try_compile(FSEEKO_COMPILE_OK "${CMAKE_BINARY_DIR}"
                            "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TestLargeFiles.c"
                            COMPILE_DEFINITIONS "-D_LARGEFILE_SOURCE" )
                if(FSEEKO_COMPILE_OK)
                    message(STATUS "Checking for fseeko/ftello - present with _LARGEFILE_SOURCE")
                    set(NEED_LARGEFILE_SOURCE 1 CACHE INTERNAL "64-bit fseeko requires _LARGEFILE_SOURCE")
                else()
                    set(FILE64_OK 0)
                    message(STATUS "64-bit off_t present but fseeko/ftello not found!")
                endif()
            endif()
        endif()

        if(FSEEKO_COMPILE_OK)
            SET(${VARIABLE} 1 CACHE INTERNAL "Result of test for large file support" FORCE)
            set(HAVE_FSEEKO 1 CACHE INTERNAL "64bit fseeko is available" FORCE)
        else()
            check_type_size("long int"      SIZEOF_LONG_INT)
            if(SIZEOF_LONG_INT EQUAL 8) #standard fseek is OK for 64bit
                SET(${VARIABLE} 1 CACHE INTERNAL "Result of test for large file support" FORCE)
            else()
                message(FATAL_ERROR "Checking for 64bit file support failed.")
            endif()
        endif()
    endif()
endmacro()
