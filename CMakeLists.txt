cmake_minimum_required (VERSION 3.1)


project (drmingw)


set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "9")
set (CPACK_PACKAGE_VERSION_PATCH "2")


option (ENABLE_COVERAGE "Enable code coverage." OFF)


##############################################################################
# Dependencies

if (NOT MINGW OR CYGWIN)
    message (FATAL_ERROR "MinGW toolchain required")
endif ()

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package (WinDbg)

# Ensure frame-pointer is never omitted on debug builds
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer")
set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer")

include_directories (
    ${CMAKE_SOURCE_DIR}/thirdparty/elf
    ${CMAKE_SOURCE_DIR}/thirdparty/dwarf
    ${CMAKE_SOURCE_DIR}/thirdparty/libiberty
    ${CMAKE_SOURCE_DIR}/thirdparty/zlib
)

add_subdirectory (thirdparty/dwarf)
add_subdirectory (thirdparty/libiberty)
add_subdirectory (thirdparty/zlib)

# Find dlltool
get_filename_component (GCC_NAME ${CMAKE_C_COMPILER} NAME)
string (REGEX REPLACE g?cc dlltool DLLTOOL_NAME ${GCC_NAME})
if (NOT DLLTOOL_NAME MATCHES dlltool)
    message (FATAL_ERROR "Best guessing the file name of dlltool failed: ${DLLTOOL_NAME}")
endif ()
find_program (DLLTOOL NAMES ${DLLTOOL_NAME})
if (DLLTOOL)
    message (STATUS "Found dlltool: ${DLLTOOL}")
else ()
    message (FATAL_ERROR "dlltool not found")
endif ()


##############################################################################
# Set global build options

include (CheckCXXCompilerFlag)

macro (add_compiler_flags)
    string (REPLACE ";" " " _FLAGS "${ARGV}")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAGS}")
endmacro ()

macro (add_linker_flags)
    string (REPLACE ";" " " _FLAGS "${ARGV}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_FLAGS}")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${_FLAGS}")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${_FLAGS}")
endmacro ()

# We require at least GCC 4.9 for decent C++11 support
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
    message (FATAL_ERROR "GCC 4.9 or newer required")
endif ()
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

# Adjust warnings
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror=implicit-function-declaration -Werror=missing-prototypes")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=missing-prototypes")

# Disable strict aliasing rules
add_compiler_flags (-fno-strict-aliasing)

# Avoid depending on MinGW runtime DLLs
check_cxx_compiler_flag (-static-libgcc HAVE_STATIC_LIBGCC_FLAG)
if (HAVE_STATIC_LIBGCC_FLAG)
    add_linker_flags (-static-libgcc)
endif ()
check_cxx_compiler_flag (-static-libstdc++ HAVE_STATIC_LIBSTDCXX_FLAG)
if (HAVE_STATIC_LIBSTDCXX_FLAG)
    add_linker_flags (-static-libstdc++)
endif ()

# Enable stack protection
add_compiler_flags (-fstack-protector-all)
# MinGW doesn't link against libssp automatically, and furthermore
# we want static linking.
set (SSP_LIBRARY "-Wl,-Bstatic -lssp -Wl,-Bdynamic")
set (CMAKE_C_STANDARD_LIBRARIES "${SSP_LIBRARY} ${CMAKE_C_STANDARD_LIBRARIES}")
set (CMAKE_CXX_STANDARD_LIBRARIES "${SSP_LIBRARY} ${CMAKE_CXX_STANDARD_LIBRARIES}")

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions (-DHAVE_WIN64=1)
else ()
    add_linker_flags (-Wl,--enable-stdcall-fixup)
    add_definitions (-DHAVE_WIN64=0)
endif ()

# Put all executables into top-level bin subdirectory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_definitions (
    # minimum required OS version
    -D_WIN32_WINNT=0x0601
    -DWINVER=0x0601
    # https://msdn.microsoft.com/en-gb/library/windows/desktop/ms683198.aspx
    -DPSAPI_VERSION=1

    # version
    -DPACKAGE_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR}
    -DPACKAGE_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR}
    -DPACKAGE_VERSION_PATCH=${CPACK_PACKAGE_VERSION_PATCH}
)

# Macro to force using debug flags, regardless of the current build type
macro (force_debug)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${CMAKE_MODULE_LINKER_FLAGS_DEBUG}")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
    foreach (build_type DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
        set (CMAKE_C_FLAGS_${build_type} "")
        set (CMAKE_CXX_FLAGS_${build_type} "")
        set (CMAKE_EXE_LINKER_FLAGS_${build_type} "")
        set (CMAKE_MODULE_LINKER_FLAGS_${build_type} "")
        set (CMAKE_SHARED_LINKER_FLAGS_${build_type} "")
    endforeach ()
endmacro ()


##############################################################################
# Targets

enable_testing ()
add_custom_target (check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
install (FILES include/exchndl.h DESTINATION include)

set (MGWHELP_IMPLIB ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libmgwhelp.a)
set (EXCHNDL_IMPLIB ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libexchndl.a)

add_subdirectory (src)
add_subdirectory (sample)
add_subdirectory (tests)


##############################################################################
# Packaging

install (
    FILES
        LICENSE.txt
        README.md
    DESTINATION doc
)

# cpack mistakenly detects Mingw-w64 as win32
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (CPACK_SYSTEM_NAME win64)
endif ()

set (CPACK_GENERATOR "7Z")

set (CPACK_STRIP_FILES ON)

include(CPack)
