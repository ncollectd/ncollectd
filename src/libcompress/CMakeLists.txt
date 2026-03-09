set(LIBCOMPRESS_SRC csnappy_compress.c csnappy_decompress.c csnappy_compat.h
                    csnappy_internal.h csnappy_internal_userspace.h
                    csnappy.h
                    slz.c slz.h tables.h
                    lz4.c lz4.h
                    compress.c compress.h)

add_library(libcompress STATIC ${LIBCOMPRESS_SRC})
set_target_properties(libcompress PROPERTIES PREFIX "")
set_target_properties(libcompress PROPERTIES POSITION_INDEPENDENT_CODE ON)
