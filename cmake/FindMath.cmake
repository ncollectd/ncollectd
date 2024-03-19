include(FindPackageHandleStandardArgs)

find_path (MATH_INCLUDE_DIR NAMES math.h)
find_library (MATH_LIBRARIES NAMES m)

find_package_handle_standard_args(Math DEFAULT_MSG MATH_LIBRARIES MATH_INCLUDE_DIR)

mark_as_advanced(MATH_INCLUDE_DIR MATH_LIBRARIES)

if(MATH_FOUND AND NOT TARGET Math::Math)
    set(MATH_INCLUDE_DIRS "${MATH_INCLUDE_DIR}")
    add_library(Math::Math INTERFACE IMPORTED)
    set_target_properties(Math::Math PROPERTIES
                          INTERFACE_INCLUDE_DIRECTORIES "${MATH_INCLUDE_DIRS}"
                          INTERFACE_LINK_LIBRARIES      "${MATH_LIBRARIES}")
endif()
