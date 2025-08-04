# https://github.com/facebook/zstd/blob/dev/build/cmake/README.md#how-to-use-it-with-cmake-fetchcontent

include (FetchContent)

set (ZSTD_BUILD_STATIC ON)
set (ZSTD_BUILD_SHARED OFF)
set (ZSTD_BUILD_COMPRESSION OFF)

FetchContent_Declare (
    zstd
    URL "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR build/cmake
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable (zstd)
