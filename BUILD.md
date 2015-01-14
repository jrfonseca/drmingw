This file has instructions on how to build DrMingw from source.


Dependencies:

 * MinGW-w64
 
 * CMake
 
 * BFD (optional)


I always build DrMinGW from Linux with MinGW cross compilation toolchain.  See
http://www.vtk.org/Wiki/CmakeMingw for details.

It is also possible to build with a native MinGW toolchain, by doing:

    set Path=C:\MinGW\bin;%Path%
    cmake -G "MinGW Makefiles" -H. -Bbuild
    cmake --build build


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
