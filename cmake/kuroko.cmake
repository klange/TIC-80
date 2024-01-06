################################
# Kuroko
################################

set(BUILD_WITH_KUROKO_DEFAULT TRUE)

option(BUILD_WITH_KUROKO "Kuroko Enabled" ${BUILD_WITH_KUROKO_DEFAULT})
message("BUILD_WITH_KUROKO: ${BUILD_WITH_KUROKO}")

if(BUILD_WITH_KUROKO)

    set(KRK_DIR ${THIRDPARTY_DIR}/kuroko/src)
    set(KRK_SRC
        ${KRK_DIR}/builtins.c
        ${KRK_DIR}/chunk.c
        ${KRK_DIR}/compiler.c
        ${KRK_DIR}/debug.c
        ${KRK_DIR}/exceptions.c
        ${KRK_DIR}/memory.c
        ${KRK_DIR}/obj_base.c
        ${KRK_DIR}/obj_bytes.c
        ${KRK_DIR}/obj_dict.c
        ${KRK_DIR}/object.c
        ${KRK_DIR}/obj_function.c
        ${KRK_DIR}/obj_gen.c
        ${KRK_DIR}/obj_list.c
        ${KRK_DIR}/obj_long.c
        ${KRK_DIR}/obj_numeric.c
        ${KRK_DIR}/obj_range.c
        ${KRK_DIR}/obj_set.c
        ${KRK_DIR}/obj_slice.c
        ${KRK_DIR}/obj_str.c
        ${KRK_DIR}/obj_tuple.c
        ${KRK_DIR}/obj_typing.c
        ${KRK_DIR}/parseargs.c
        ${KRK_DIR}/scanner.c
        ${KRK_DIR}/sys.c
        ${KRK_DIR}/table.c
        ${KRK_DIR}/threads.c
        ${KRK_DIR}/value.c
        ${KRK_DIR}/vm.c
    )

    add_library(kuroko STATIC ${KRK_SRC})

    target_include_directories(kuroko PUBLIC ${THIRDPARTY_DIR}/kuroko/src)
    target_compile_definitions(kuroko PUBLIC  KRK_STATIC_ONLY=1)
    target_compile_definitions(kuroko PUBLIC  KRK_NO_FILESYSTEM=1)
    target_compile_definitions(kuroko PUBLIC  KRK_NO_TRACING=1)
    target_compile_definitions(kuroko PUBLIC  KRK_NO_DISASSEMBLY=1)
    target_compile_definitions(kuroko PUBLIC  KRK_DISABLE_DEBUG=1)
    target_compile_definitions(kuroko PUBLIC  KRK_DISABLE_THREADS=1)
    target_compile_definitions(kuroko PUBLIC  KRK_NO_GC_TRACING=1)
    target_compile_definitions(kuroko PUBLIC  KRK_NO_CALLGRIND=1)
    target_compile_definitions(kuroko INTERFACE TIC_BUILD_WITH_KUROKO=1)

endif()

