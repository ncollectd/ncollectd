include(FindPackageHandleStandardArgs)

function(perl_get_info _pgi_info tag)
  cmake_parse_arguments(_PGI "IS_PATH" "" "" ${ARGN})

  set(${_pgi_info} NOTFOUND PARENT_SCOPE)

  execute_process(COMMAND "${PERL_EXECUTABLE}" -V:${tag}
                  OUTPUT_VARIABLE result
                  RESULT_VARIABLE status)

  if(NOT status)
    string(REGEX REPLACE "${tag}='([^']*)'.*" "\\1" result "${result}")
    if(_PGI_IS_PATH)
      file(TO_CMAKE_PATH "${result}" result)
    endif()
    set(${_pgi_info} "${result}" PARENT_SCOPE)
  endif()
endfunction()

find_program(PERL_EXECUTABLE NAMES perl)

if (PERL_EXECUTABLE)
  execute_process(COMMAND ${PERL_EXECUTABLE} -V:version
                  OUTPUT_VARIABLE PERL_VERSION_OUTPUT_VARIABLE
                  RESULT_VARIABLE PERL_VERSION_RESULT_VARIABLE
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT PERL_VERSION_RESULT_VARIABLE AND NOT PERL_VERSION_OUTPUT_VARIABLE MATCHES "^version='UNKNOWN'")
    string(REGEX REPLACE "version='([^']+)'.*" "\\1" PERL_VERSION_STRING ${PERL_VERSION_OUTPUT_VARIABLE})
  else()
    execute_process(COMMAND ${PERL_EXECUTABLE} -v
                    OUTPUT_VARIABLE PERL_VERSION_OUTPUT_VARIABLE
                    RESULT_VARIABLE PERL_VERSION_RESULT_VARIABLE
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT PERL_VERSION_RESULT_VARIABLE AND PERL_VERSION_OUTPUT_VARIABLE MATCHES "This is perl.*[ \\(]v([0-9\\._]+)[ \\)]")
      set(PERL_VERSION_STRING "${CMAKE_MATCH_1}")
    elseif(NOT PERL_VERSION_RESULT_VARIABLE AND PERL_VERSION_OUTPUT_VARIABLE MATCHES "This is perl, version ([0-9\\._]+) +")
      set(PERL_VERSION_STRING "${CMAKE_MATCH_1}")
    endif()
  endif()

  perl_get_info(PERL_ARCHNAME archname)
  perl_get_info(PERL_EXTRA_C_FLAGS cppflags)
  perl_get_info(PERL_ARCHLIB archlib IS_PATH)
  perl_get_info(PERL_UPDATE_ARCHLIB installarchlib IS_PATH)
  perl_get_info(PERL_POSSIBLE_LIBRARY_NAMES libperl)

  if (NOT PERL_POSSIBLE_LIBRARY_NAMES)
    set(PERL_POSSIBLE_LIBRARY_NAMES perl${PERL_VERSION_STRING} perl)
  endif()

  find_path(LIBPERL_INCLUDE_PATH NAMES perl.h
            PATHS "${PERL_UPDATE_ARCHLIB}/CORE"
                  "${PERL_ARCHLIB}/CORE"
                  /usr/lib/perl5/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
                  /usr/lib/perl/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
                  /usr/lib/perl5/${PERL_VERSION_STRING}/CORE
                  /usr/lib/perl/${PERL_VERSION_STRING}/CORE)

  find_library(LIBPERL_LIBRARY NAMES ${PERL_POSSIBLE_LIBRARY_NAMES}
               PATHS "${PERL_UPDATE_ARCHLIB}/CORE"
                     "${PERL_ARCHLIB}/CORE"
                     /usr/lib/perl5/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
                     /usr/lib/perl/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
                     /usr/lib/perl5/${PERL_VERSION_STRING}/CORE
                     /usr/lib/perl/${PERL_VERSION_STRING}/CORE)
endif ()

find_package_handle_standard_args(LibPerl REQUIRED_VARS LIBPERL_LIBRARY LIBPERL_INCLUDE_PATH)

mark_as_advanced(LIBPERL_INCLUDE_PATH LIBPERL_LIBRARY)

if(LIBPERL_FOUND AND NOT TARGET LibPerl::LibPerl)
    set(LIBPERL_INCLUDE_DIRS "${LIBPERL_INCLUDE_DIR}")
    string(REPLACE " " ";" PERL_EXTRA_C_FLAGS ${PERL_EXTRA_C_FLAGS})
    add_library(LibPerl::LibPerl INTERFACE IMPORTED)
    set_target_properties(LibPerl::LibPerl PROPERTIES
                          INTERFACE_COMPILE_OPTIONS     "${PERL_EXTRA_C_FLAGS}"
                          INTERFACE_INCLUDE_DIRECTORIES "${LIBPERL_INCLUDE_PATH}"
                          INTERFACE_LINK_LIBRARIES      "${LIBPERL_LIBRARY}")
endif()
