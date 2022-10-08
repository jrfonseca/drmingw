#!/bin/sh

set -eux

xvfb_run() {
	xvfb-run -a -s '-screen 0 1024x768x24' "$@"
}


#
# Download and setup llvm-ming
#

release=20220906
basename=llvm-mingw-$release-ucrt-ubuntu-18.04-x86_64
archive=$basename.tar.xz

mkdir -p downloads
test -f downloads/$archive || wget -q -O downloads/$archive https://github.com/mstorsjo/llvm-mingw/releases/download/$release/$archive
test -d downloads/$basename || tar -xJf downloads/$archive -C downloads

rm -f downloads/llvm-mingw
ln -sf $basename downloads/llvm-mingw

export PATH="$PWD/downloads/llvm-mingw/bin:$PATH"


#
# Setup WINE
#

WINE=${WINE:-$(which wine)}

mkdir -p build

export WINEPREFIX=$PWD/build/wine

# Prevent Gecko/Mono installation dialogues
# https://forum.winehq.org/viewtopic.php?f=2&t=16320#p78458
export WINEDLLOVERRIDES="mscoree,mshtml="

test -d $WINEPREFIX || xvfb_run $WINE wineboot.exe --init

export WINEDEBUG="${WINEDEBUG:-+debugstr}"

#export WINEPATH="$PWD/downloads/llvm-mingw/x86_64-w64-mingw32/bin;$PWD/downloads/llvm-mingw/i686-w64-mingw32/bin"


#
# Build
#

CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Debug}
if [ "${GITHUB_EVENT_NAME:-}" = "push" ]
then
	case "${GITHUB_REF:-}" in
	refs/tags/*)
		CMAKE_BUILD_TYPE=Release
		;;
	esac
fi

for target in \
	x86_64-w64-mingw32-clang \
	i686-w64-mingw32-clang \
	aarch64-w64-mingw32-clang \
	armv7-w64-mingw32-clang
do
	toolchain=$PWD/ci/toolchain/$target.cmake

	test -f build/$target/CMakeCache.txt || cmake -S . -B build/$target -G Ninja --toolchain $toolchain -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE
	cmake --build build/$target --target all --target package -- "$@"
	
	test -f build/apps/$target/CMakeCache.txt || cmake -S tests/apps -B build/apps/$target -G Ninja --toolchain $toolchain -DCMAKE_BUILD_TYPE=Debug
	cmake --build build/apps/$target --target all -- "$@"
done

xvfb_run python3 tests/apps/test.py -w $WINE build/x86_64-w64-mingw32-clang/bin/catchsegv.exe build/apps/x86_64-w64-mingw32-clang build/apps/i686-w64-mingw32-clang
xvfb_run python3 tests/apps/test.py -w $WINE build/i686-w64-mingw32-clang/bin/catchsegv.exe build/apps/i686-w64-mingw32-clang
