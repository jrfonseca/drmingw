# Dependencies #

Required:

 * [MinGW-w64](http://mingw-w64.sourceforge.net/) toolchain

   * _win32_ threads (as opposed to _posix_ threads) is recommended to avoid dependency on `libwinpthread-1.dll`
 
 * [CMake](http://www.cmake.org/)
 

Optional:

 * [Debugging Tools for Windows](https://msdn.microsoft.com/en-us/library/windows/hardware/ff551063.aspx)
   for the latest version of `dbghelp.dll` and `symsrv.dll` DLLs.

 * [BFD](http://www.gnu.org/software/binutils/)


# Cross-compilation toolchain #

I always build DrMinGW from Linux with MinGW cross-compilation toolchain.  See
[here](http://www.vtk.org/Wiki/CmakeMingw) for details.

## BFD ##

BFD headers are not typically included in native/cross MinGW distributions.
Binaries are but they too have several undesired dependencies.  So the best
approach is to build BFD from source.

These are roughly the steps (again, assuming a cross-compilation MinGW
toolchain):

    wget http://ftp.gnu.org/gnu/binutils/binutils-2.23.2.tar.bz2
    tar -xjf binutils-2.23.2.tar.bz2
    cd binutils-2.23.2
    ./configure --host i686-w64-mingw32 --disable-nls --prefix=/
    make
    make install DESTDIR=$PWD/publish


# Native toolchain #

It is also possible to build with a native MinGW toolchain, by doing:

    set Path=C:\path\to\mingw32\bin;%Path%
    cmake -G "MinGW Makefiles" -H. -Bbuild
    cmake --build build

These instructions have been tested with
[this MinGW-w64 toolchain](http://sourceforge.net/projects/mingwbuilds/files/host-windows/releases/4.8.1/32-bit/threads-win32/dwarf/x32-4.8.1-release-win32-dwarf-rev5.7z/download),
but in theory it should work with any flavour of MinGW-w64 native toolchain,
provided that it includes a native `mingw32-make.exe`.

Note that building with MSYS or Cygwin is not necessary nor *supported*.
