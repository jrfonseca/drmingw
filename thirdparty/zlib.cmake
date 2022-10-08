add_library (z EXCLUDE_FROM_ALL
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/gzclose.c
    zlib/gzlib.c
    zlib/gzread.c
    zlib/gzwrite.c
    zlib/inflate.c
    zlib/infback.c
    zlib/inftrees.c
    zlib/inffast.c
    zlib/trees.c
    zlib/uncompr.c
    zlib/zutil.c
)

# adjust warnings
if (MSVC)
    target_compile_options (z PRIVATE -wd4131) # uses old-style declarator
else ()
    target_compile_definitions (z PRIVATE HAVE_UNISTD_H)
    if (CMAKE_C_COMPILER_ID MATCHES Clang)
        target_compile_options (z PRIVATE -Wno-deprecated-non-prototype)
    endif ()
endif ()

target_include_directories (z PUBLIC zlib)

install (
    FILES zlib/README
    DESTINATION doc
    RENAME LICENSE-zlib.txt
)
