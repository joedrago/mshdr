project(mshdr)

set(MSHDR_SRCS
    src/main.cpp
    src/image.cpp
    src/image.h
)

include_directories(src)

add_executable(mshdr WIN32 ${MSHDR_SRCS})
