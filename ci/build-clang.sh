#!/bin/sh

set -eux

xvfb_run() {
	xvfb-run -a -s '-screen 0 1024x768x24' "$@"
}


. ci/dependencies-clang.sh


#
# Download and setup llvm-ming
#

mkdir -p downloads
test -f downloads/$llvm_archive || wget -q -O downloads/$llvm_archive https://github.com/mstorsjo/llvm-mingw/releases/download/$llvm_release/$llvm_archive
test -d downloads/$llvm_basename || tar -xJf downloads/$llvm_archive -C downloads

rm -f downloads/llvm-mingw
ln -sf $llvm_basename downloads/llvm-mingw

export PATH="$PWD/downloads/llvm-mingw/bin:$PATH"


#
# Setup WINE
#

WINE=${WINE:-$(which wine)}

BUILD_DIR=${BUILD_DIR:-$PWD/build}

mkdir -p $BUILD_DIR

export WINEPREFIX=$BUILD_DIR/wine

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
	aarch64-w64-mingw32-clang
do
	toolchain=$PWD/ci/toolchain/$target.cmake

	case $target in
	x86_64-w64-mingw32-clang|i686-w64-mingw32-clang)
		CMAKE_CROSSCOMPILING_EMULATOR=$WINE
		;;
	*)
		CMAKE_CROSSCOMPILING_EMULATOR=
		;;
	esac

	test -f $BUILD_DIR/$target/CMakeCache.txt || cmake -S . -B $BUILD_DIR/$target -G Ninja --toolchain $toolchain -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_CROSSCOMPILING_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}
	cmake --build $BUILD_DIR/$target --target all --target check --target package

	python3 tests/check_dynamic_linkage.py --objdump="${target%%clang}objdump" --validate $BUILD_DIR/$target/bin/*.dll $BUILD_DIR/$target/bin/*.exe

	test -f $BUILD_DIR/apps/$target/CMakeCache.txt || cmake -S tests/apps -B $BUILD_DIR/apps/$target -G Ninja --toolchain $toolchain -DCMAKE_BUILD_TYPE=Debug
	cmake --build $BUILD_DIR/apps/$target --target all
done

xvfb_run python3 tests/apps/test.py -w $WINE "$@" $BUILD_DIR/x86_64-w64-mingw32-clang/bin/catchsegv.exe $BUILD_DIR/apps/x86_64-w64-mingw32-clang $BUILD_DIR/apps/i686-w64-mingw32-clang
xvfb_run python3 tests/apps/test.py -w $WINE "$@" $BUILD_DIR/i686-w64-mingw32-clang/bin/catchsegv.exe $BUILD_DIR/apps/i686-w64-mingw32-clang
