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


## Cross-compilation toolchain

I always build DrMinGW from Linux with MinGW cross-compilation toolchain.  See
[here](http://www.vtk.org/Wiki/CmakeMingw) for details.


## Native toolchain

It is also possible to build with a native MinGW toolchain, by doing:

    set Path=C:\path\to\mingw\bin;%Path%
    cmake -G "MinGW Makefiles" -H. -Bbuild
    cmake --build build

These instructions have been tested with the following MinGW-w64 toolchains:

 * [32-bits](https://downloads.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/7.3.0/threads-win32/dwarf/i686-7.3.0-release-win32-dwarf-rt_v5-rev0.7z)

 * [64-bits](https://downloads.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/7.3.0/threads-win32/seh/x86_64-7.3.0-release-win32-seh-rt_v5-rev0.7z)

but in theory it should work with any flavour of MinGW-w64 native toolchain,
provided that it includes a native `mingw32-make.exe`.

Note that building with MSYS or Cygwin is not necessary nor *supported*.
