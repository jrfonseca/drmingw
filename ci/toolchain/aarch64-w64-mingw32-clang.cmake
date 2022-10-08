# https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html
set (CMAKE_SYSTEM_NAME Windows)
set (CMAKE_SYSTEM_PROCESSOR ARM64)
set (CMAKE_C_COMPILER aarch64-w64-mingw32-clang)
set (CMAKE_CXX_COMPILER aarch64-w64-mingw32-clang++)
set (CMAKE_RC_COMPILER aarch64-w64-mingw32-windres)
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Statically link CRT
set (CMAKE_C_FLAGS "--start-no-unused-arguments -Wl,-Bstatic -lunwind -Wl,-Bdynamic --end-no-unused-arguments")
set (CMAKE_CXX_FLAGS "--start-no-unused-arguments -static-libstdc++ -Wl,-Bstatic -lunwind -Wl,-Bdynamic --end-no-unused-arguments")
