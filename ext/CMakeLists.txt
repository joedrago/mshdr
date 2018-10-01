set(ZLIB_SRCS
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/deflate.c
    # zlib/gzclose.c
    # zlib/gzlib.c
    # zlib/gzread.c
    # zlib/gzwrite.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/uncompr.c
    zlib/zutil.c
)
include_directories(zlib)
add_library(zlib ${ZLIB_SRCS})
set_target_properties(zlib PROPERTIES FOLDER ext)

set(LIBPNG_SRCS
    libpng/png.c
    libpng/pngerror.c
    libpng/pngget.c
    libpng/pngmem.c
    #libpng/pngpread.c
    libpng/pngread.c
    libpng/pngrio.c
    libpng/pngrtran.c
    libpng/pngrutil.c
    libpng/pngset.c
    #libpng/pngtest.c
    libpng/pngtrans.c
    libpng/pngwio.c
    libpng/pngwrite.c
    libpng/pngwtran.c
    libpng/pngwutil.c
)
include_directories(zlib libpng)
add_library(libpng ${LIBPNG_SRCS})
set_target_properties(libpng PROPERTIES FOLDER ext)
