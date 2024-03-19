# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Copyright (c) 2009, Michihiro NAKAJIMA
# SPDX-FileCopyrightText: Copyright (c) 2006, Alexander Neundorf
# SPDX-FileContributor Michihiro NAKAJIMA
# SPDX-FileContributor Alexander Neundorf <neundorf at kde.org>

include(CheckCSourceCompiles)

macro(check_type_exists _TYPE _HEADER _RESULT)
   set(_INCLUDE_FILES)
   foreach(it ${_HEADER})
      set(_INCLUDE_FILES "${_INCLUDE_FILES}#include <${it}>\n")
   endforeach()

   set(_CHECK_TYPE_EXISTS_SOURCE_CODE "
${_INCLUDE_FILES}
int main()
{
   static ${_TYPE} tmp;
   if (sizeof(tmp))
      return 0;
  return 0;
}
")
    check_c_source_compiles("${_CHECK_TYPE_EXISTS_SOURCE_CODE}" ${_RESULT})
endmacro()
