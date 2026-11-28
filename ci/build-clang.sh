#!/bin/bash

set -eux

. ci/common.sh
. ci/dependencies-clang.sh


#
# Download and setup llvm-mingw
#

mkdir -p downloads
test -f downloads/$llvm_archive || wget -q -O downloads/$llvm_archive https://github.com/mstorsjo/llvm-mingw/releases/download/$llvm_release/$llvm_archive
test -d downloads/$llvm_basename || tar -xJf downloads/$llvm_archive -C downloads

rm -f downloads/llvm-mingw
ln -sf $llvm_basename downloads/llvm-mingw

export PATH="$PWD/downloads/llvm-mingw/bin:$PATH"


#
# Build
#

targets="x86_64-w64-mingw32-clang i686-w64-mingw32-clang"
if [ "$COVERAGE" -eq 0 ]
then
	# llvm-mingw ships no libclang_rt.profile for aarch64, and there is no
	# emulator to gather coverage data with anyway
	targets="$targets aarch64-w64-mingw32-clang"
fi

for target in $targets
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

	cmake_ -B $BUILD_DIR/$target -S . -G Ninja --toolchain $toolchain -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_CROSSCOMPILING_EMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR} -DENABLE_COVERAGE=$COVERAGE
	coverage_reset $BUILD_DIR/$target
	cmake --build $BUILD_DIR/$target --target all --target check --target package

	python3 tests/check_dynamic_linkage.py --objdump="${target%%clang}objdump" --validate $BUILD_DIR/$target/bin/*.dll $BUILD_DIR/$target/bin/*.exe

	cmake_ -B $BUILD_DIR/apps/$target -S tests/apps -G Ninja --toolchain $toolchain -DCMAKE_BUILD_TYPE=Debug
	cmake --build $BUILD_DIR/apps/$target --target all
done

status=0
xwfb_run python3 tests/apps/test.py -w $WINE "$@" $BUILD_DIR/x86_64-w64-mingw32-clang/bin/catchsegv.exe $BUILD_DIR/apps/x86_64-w64-mingw32-clang $BUILD_DIR/apps/i686-w64-mingw32-clang || status=1
xwfb_run python3 tests/apps/test.py -w $WINE "$@" $BUILD_DIR/i686-w64-mingw32-clang/bin/catchsegv.exe $BUILD_DIR/apps/i686-w64-mingw32-clang || status=1

coverage_report $BUILD_DIR/x86_64-w64-mingw32-clang "llvm-cov gcov"
coverage_report $BUILD_DIR/i686-w64-mingw32-clang "llvm-cov gcov"

if [ "$status" -ne "0" ]
then
	echo Failed
fi

exit $status
