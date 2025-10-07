set(LIBQUICKJS_SRC quickjs.c quickjs.h quickjs-opcode.h quickjs-atom.h
                   quickjs-libc.c quickjs-libc.h
                   cutils.c cutils.h list.h
                   dtoa.c dtoa.h
                   libregexp.c libregexp.h libregexp-opcode.h
                   libunicode.c libunicode.h libunicode-table.h unicode_gen_def.h)

add_library(libquickjs STATIC ${LIBQUICKJS_SRC})
set_target_properties(libquickjs PROPERTIES PREFIX "")
set_target_properties(libquickjs PROPERTIES POSITION_INDEPENDENT_CODE ON)
