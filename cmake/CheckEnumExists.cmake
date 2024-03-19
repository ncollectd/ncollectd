# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright (c) 2009, Michihiro NAKAJIMA
# SPDX-FileCopyrightText: Copyright (c) 2006, Alexander Neundorf
# SPDX-FileContributor Michihiro NAKAJIMA
# SPDX-FileContributor Alexander Neundorf <neundorf at kde.org>

include(CheckCSourceCompiles)

macro(check_enum_exists _ENUM _HEADER _RESULT)
   set(_INCLUDE_FILES)
   foreach(it ${_HEADER})
      set(_INCLUDE_FILES "${_INCLUDE_FILES}#include <${it}>\n")
   endforeach(it)

   set(_CHECK_ENUM_EXISTS_SOURCE_CODE "
${_INCLUDE_FILES}
int main()
{
   int tmp = ${_ENUM};
    return 0;
} ")
    check_c_source_compiles("${_CHECK_ENUM_EXISTS_SOURCE_CODE}" ${_RESULT})
endmacro()
