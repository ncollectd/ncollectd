include(FindPackageHandleStandardArgs)

foreach(PY_CONFIG ${PYTHON_CONFIG} "python3-config" "python2-config" "python-config")
    find_program(PY_CONFIG_FOUND "${PY_CONFIG}")
    if(PY_CONFIG_FOUND)
        execute_process(COMMAND "${PY_CONFIG}" "--embed"
                        OUTPUT_VARIABLE PY_CONFIG_TEST
                        RESULT_VARIABLE PY_CONFIG_CODE
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if("${PY_CONFIG_CODE}" EQUAL 0)
            set(PY_CONFIG_EMBED 1)
        else()
            set(PY_CONFIG_EMBED 0)
        endif()

        execute_process(COMMAND "${PY_CONFIG}" "--includes"
                        OUTPUT_VARIABLE PY_CONFIG_INCLUDES
                        RESULT_VARIABLE PY_CONFIG_CODE
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if("${PY_CONFIG_CODE}" EQUAL 0)
            string(REPLACE " " ";" PY_CONFIG_INCLUDES "${PY_CONFIG_INCLUDES}")
            foreach(PY_CONFIG_ITEM LIST ${PY_CONFIG_INCLUDES})
                string(FIND ${PY_CONFIG_ITEM} "-I" PY_CONFIG_ITEM_POS)
                if("${PY_CONFIG_ITEM_POS}" EQUAL 0)
                    string(REPLACE "-I" "" PY_CONFIG_ITEM "${PY_CONFIG_ITEM}")
                    list(APPEND LIBPYTHON_INCLUDE_DIRS "${PY_CONFIG_ITEM}")
                endif()
            endforeach()
        endif()

        if(PY_CONFIG_EMBED)
            set(PY_CONFIG_FLAGS "--ldflags" "--embed")
        else()
            set(PY_CONFIG_FLAGS "--ldflags")
        endif()
        execute_process(COMMAND "${PY_CONFIG}" ${PY_CONFIG_FLAGS}
                        OUTPUT_VARIABLE PY_CONFIG_LDFLAGS
                        RESULT_VARIABLE PY_CONFIG_CODE
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if("${PY_CONFIG_CODE}" EQUAL 0)
            string(REPLACE " " ";" PY_CONFIG_LDFLAGS "${PY_CONFIG_LDFLAGS}")
            foreach(PY_CONFIG_ITEM LIST ${PY_CONFIG_LDFLAGS})
                string(FIND ${PY_CONFIG_ITEM} "-L" PY_CONFIG_ITEM_POS)
                if("${PY_CONFIG_ITEM_POS}" EQUAL 0)
                    string(REPLACE "-L" "" PY_CONFIG_ITEM "${PY_CONFIG_ITEM}")
                    list(APPEND PY_CONFIG_CFLAGS "-L${PY_CONFIG_ITEM}")
                    list(APPEND PY_CONFIG_LIBRARY_DIRS "${PY_CONFIG_ITEM}")
                endif()
            endforeach()
        endif()

        if(PY_CONFIG_EMBED)
            set(PY_CONFIG_FLAGS "--libs" "--embed")
        else()
            set(PY_CONFIG_FLAGS "--libs")
        endif()
        execute_process(COMMAND "${PY_CONFIG}" ${PY_CONFIG_FLAGS}
                        OUTPUT_VARIABLE PY_CONFIG_LIBS
                        RESULT_VARIABLE PY_CONFIG_CODE
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if("${PY_CONFIG_CODE}" EQUAL 0)
            string(REPLACE " " ";" PY_CONFIG_LIBS "${PY_CONFIG_LIBS}")
            foreach(PY_CONFIG_ITEM LIST ${PY_CONFIG_LIBS})
                string(FIND ${PY_CONFIG_ITEM} "-l" PY_CONFIG_ITEM_POS)
                if("${PY_CONFIG_ITEM_POS}" EQUAL 0)
                    string(REPLACE "-l" "" PY_CONFIG_ITEM "${PY_CONFIG_ITEM}")
                    string(FIND ${PY_CONFIG_ITEM} "python" PY_CONFIG_ITEM_POS)
                    if("${PY_CONFIG_ITEM_POS}" EQUAL 0)
                        set(PYTHON_LIB "${PY_CONFIG_ITEM}")
                    else()
                        list(APPEND PY_CONFIG_CFLAGS "-l${PY_CONFIG_ITEM}")
                    endif()
                endif()
            endforeach()
        endif()

        break()
    endif()
endforeach()

if(NOT PYTHON_LIB)
    set(PYTHON_LIB python)
endif()

find_path(LIBPYTHON_INCLUDE_DIR NAMES Python.h HINTS ${PY_CONFIG_INCLUDE_DIRS})
find_library(LIBPYTHON_LIBRARIES NAMES "${PYTHON_LIB}" HINTS ${PY_CONFIG_LIBRARY_DIRS})

find_package_handle_standard_args(LibPython REQUIRED_VARS LIBPYTHON_LIBRARIES LIBPYTHON_INCLUDE_DIRS)

mark_as_advanced(LIBPYTHON_INCLUDE_DIR LIBPYTHON_LIBRARIES)

if(LIBPYTHON_FOUND AND NOT TARGET LibPython::LibPython)
    add_library(LibPython::LibPython INTERFACE IMPORTED)
    set_target_properties(LibPython::LibPython PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${PY_CONFIG_CFLAGS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBPYTHON_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${LIBPYTHON_LIBRARIES}")
endif()
