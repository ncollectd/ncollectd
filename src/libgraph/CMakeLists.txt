set(LIBGRAPH_SRC graph.c graph.h
                 graph_gfx.h
                 graph_gfx_svg.c
                 parsetime.c
                 image.c
                 spng.c spng.h
                 schrift.c schrift.h)

add_library(libgraph STATIC ${LIBGRAPH_SRC})
set_target_properties(libgraph PROPERTIES PREFIX "")
set_target_properties(libgraph PROPERTIES POSITION_INDEPENDENT_CODE ON)
