project(mshdr)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(ext)

include_directories(
    ext/zlib
    ext/libpng
)

set(MSHDR_SRCS
    src/main.cpp
    src/image.cpp
    src/image.h
)

include_directories(src)

add_executable(mshdr WIN32 ${MSHDR_SRCS})
target_link_libraries(mshdr zlib libpng)
