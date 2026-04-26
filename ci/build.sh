#!/bin/sh

set -eux

cmake_ () {
	test "$1" = "-B"
	test -f "$2/CMakeCache.txt" || cmake "$@"
}

cmake_mingw32() {
	cmake_ "$@" --toolchain $PWD/ci/toolchain/i686-w64-mingw32-gcc.cmake
}
cmake_mingw64() {
	cmake_ "$@" --toolchain $PWD/ci/toolchain/x86_64-w64-mingw32-gcc.cmake
}

xvfb_run() {
	# Make Xvfb more tolerant on CI and avoid flaky X11/GL paths.
	xvfb-run -a -s '-screen 0 1024x768x24 -nolisten tcp -noreset +extension RANDR +extension GLX' "$@"
}

test ! -d /usr/lib/ccache || export PATH="/usr/lib/ccache:$PATH"


#
# Setup WINE
#

WINE=${WINE:-$(which wine)}

x86_64-w64-mingw32-g++-win32 --version
i686-w64-mingw32-g++-win32 --version
ninja --version
cmake --version
python3 --version
$WINE --version

BUILD_DIR=${BUILD_DIR:-$PWD/build}

mkdir -p $BUILD_DIR

export WINEPREFIX=$BUILD_DIR/wine

# Prevent Gecko/Mono installation dialogues
# https://forum.winehq.org/viewtopic.php?f=2&t=16320#p78458
export WINEDLLOVERRIDES="mscoree,mshtml="

if ! test -d $WINEPREFIX
then
	xvfb_run $WINE wineboot.exe --init
	$WINE reg.exe ADD 'HKCU\Software\Wine\winedbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f
fi

export WINEDEBUG="${WINEDEBUG:-+debugstr}"



#
# Build
#

CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug}
cmake_mingw64 -B $BUILD_DIR/mingw64 -S . -G Ninja -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_CROSSCOMPILING_EMULATOR=$WINE
cmake_mingw32 -B $BUILD_DIR/mingw32 -S . -G Ninja -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_CROSSCOMPILING_EMULATOR=$WINE
cmake --build $BUILD_DIR/mingw64 --target all
cmake --build $BUILD_DIR/mingw32 --target all
python3 tests/check_dynamic_linkage.py --objdump=x86_64-w64-mingw32-objdump --validate $BUILD_DIR/mingw64/bin/*.dll $BUILD_DIR/mingw64/bin/*.exe
python3 tests/check_dynamic_linkage.py --objdump=i686-w64-mingw32-objdump --validate $BUILD_DIR/mingw32/bin/*.dll $BUILD_DIR/mingw32/bin/*.exe
xvfb_run cmake --build $BUILD_DIR/mingw64 --target check
xvfb_run cmake --build $BUILD_DIR/mingw32 --target check

cmake_mingw64 -B $BUILD_DIR/apps/mingw64 -S tests/apps -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build $BUILD_DIR/apps/mingw64 --target all
cmake_mingw32 -B $BUILD_DIR/apps/mingw32 -S tests/apps -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build $BUILD_DIR/apps/mingw32 --target all

status=0
xvfb_run python3 tests/apps/test.py -w $WINE "$@" $BUILD_DIR/mingw64/bin/catchsegv.exe $BUILD_DIR/apps/mingw64 $BUILD_DIR/apps/mingw32 || status=1
xvfb_run python3 tests/apps/test.py -w $WINE "$@" $BUILD_DIR/mingw32/bin/catchsegv.exe $BUILD_DIR/apps/mingw32 || status=1

if [ "$status" -ne "0" ]
then
	echo Failed
fi

exit $status
