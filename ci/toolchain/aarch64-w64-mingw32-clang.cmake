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
set (CMAKE_C_STANDARD_LIBRARIES "-Wl,-Bstatic -lunwind -Wl,-Bdynamic")
set (CMAKE_CXX_STANDARD_LIBRARIES "-static-libstdc++ ${CMAKE_C_STANDARD_LIBRARIES}")
