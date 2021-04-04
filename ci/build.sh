#!/bin/sh

set -eux

build () {
	test "$1" = "-B"
	test -f "$2/CMakeCache.txt" || cmake "$@"
}

build_mingw32() {
	build "$@" -DCMAKE_TOOLCHAIN_FILE=$PWD/.github/scripts/mingw32.cmake
}
build_mingw64() {
	build "$@" -DCMAKE_TOOLCHAIN_FILE=$PWD/.github/scripts/mingw64.cmake
}

xvfb_run() {
	xvfb-run -a -s '-screen 0 1024x768x24' "$@"
}

WINE=${WINE:-$(which wine)}

x86_64-w64-mingw32-g++ --version
i686-w64-mingw32-g++ --version
ninja --version
cmake --version
python3 --version
$WINE --version

mkdir -p build

export WINEPREFIX=$PWD/build/wine

# Prevent Gecko/Mono installation dialogues
# https://forum.winehq.org/viewtopic.php?f=2&t=16320#p78458
export WINEDLLOVERRIDES="mscoree,mshtml="

test -d $WINEPREFIX || xvfb_run $WINE wineboot.exe --init

build_mingw64 -B build/mingw64 -S . -G Ninja -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug} -DWINE_PROGRAM=$WINE
build_mingw32 -B build/mingw32 -S . -G Ninja -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug} -DWINE_PROGRAM=$WINE
cmake --build build/mingw64 --target all
cmake --build build/mingw32 --target all
xvfb_run cmake --build build/mingw64 --target check
xvfb_run cmake --build build/mingw32 --target check

build_mingw64 -B build/apps/mingw64 -S tests/apps -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/apps/mingw64 --target all
build_mingw32 -B build/apps/mingw32 -S tests/apps -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/apps/mingw32 --target all

status=0
xvfb_run python3 tests/apps/test.py -w $WINE "$@" build/mingw64/bin/catchsegv.exe build/apps/mingw64 build/apps/mingw32 || status=1
xvfb_run python3 tests/apps/test.py -w $WINE "$@" build/mingw32/bin/catchsegv.exe build/apps/mingw32 || status=1

if [ "$status" -ne "0" ]
then
	echo Failed
fi

exit $status
