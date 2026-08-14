set(LIBTEST_SRC plugin.c
                metric.c
                testing.c testing.h)

add_library(libtest STATIC ${LIBTEST_SRC})
set_target_properties(libtest PROPERTIES PREFIX "")
set_target_properties(libtest PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(libtest INTERFACE libmetric pthread)
# add_dependencies(libtest libmetric)
