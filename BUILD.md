# Build Instructions


## Dependencies

Required:

 * [MinGW-w64](http://mingw-w64.sourceforge.net/) toolchain

   * _win32_ threads (as opposed to _posix_ threads) is recommended to avoid dependency on `libwinpthread-1.dll`

 * [CMake](http://www.cmake.org/)

Recommended:

 * [Debugging Tools for Windows](https://msdn.microsoft.com/en-us/library/windows/hardware/ff551063.aspx)
   for the latest version of `dbghelp.dll`, `dbgcore.dll`, and `symsrv.dll` DLLs.

 * [Python 3.x](https://www.python.org/downloads/) for running some of the tests.


## Native toolchain

It is also possible to build with a native MinGW toolchain, by doing:

    powershell -NoProfile -ExecutionPolicy Bypass -File ci\build.ps1

Note that building with MSYS or Cygwin is not necessary nor *supported*.


## Cross-compilation toolchain

I always build DrMinGW from Linux with MinGW cross-compilation toolchain, by
doing:

    ci/build.sh
