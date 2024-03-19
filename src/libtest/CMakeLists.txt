set(LIBTEST_SRC plugin.c
                metric.c
                testing.h)

add_library(libtest STATIC ${LIBTEST_SRC})
set_target_properties(libtest PROPERTIES PREFIX "")
set_target_properties(libtest PROPERTIES POSITION_INDEPENDENT_CODE ON)
# add_dependencies(libtest libmetric)
