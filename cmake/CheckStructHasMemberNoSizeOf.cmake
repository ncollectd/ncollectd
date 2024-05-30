# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright 2000-2024 Kitware, Inc. and Contributors

include(CheckCSourceCompiles)

macro (CHECK_STRUCT_HAS_MEMBER_NO_SIZEOF _STRUCT _MEMBER _HEADER _RESULT)
    set(_INCLUDE_FILES)
    foreach (it ${_HEADER})
        string(APPEND _INCLUDE_FILES "#include <${it}>\n")
    endforeach ()

    if("x${ARGN}" STREQUAL "x")
        set(_lang C)
    elseif("x${ARGN}" MATCHES "^xLANGUAGE;([a-zA-Z]+)$")
        set(_lang "${CMAKE_MATCH_1}")
    else()
        message(FATAL_ERROR "Unknown arguments:\n  ${ARGN}\n")
    endif()

    set(_CHECK_STRUCT_MEMBER_SOURCE_CODE "
${_INCLUDE_FILES}
int main()
{
  ${_STRUCT}* tmp;
  tmp->${_MEMBER};
  return 0;
}
")

    check_c_source_compiles("${_CHECK_STRUCT_MEMBER_SOURCE_CODE}" ${_RESULT})
endmacro ()
