cmake_minimum_required (VERSION 2.8)


project (drmingw)


set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "6")
set (CPACK_PACKAGE_VERSION_PATCH "6")


##############################################################################
# Dependencies

if (NOT MINGW OR CYGWIN)
    message (FATAL_ERROR "MinGW toolchain required")
endif ()

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package (WinDbg)

include_directories (
    ${CMAKE_SOURCE_DIR}/thirdparty/elf
    ${CMAKE_SOURCE_DIR}/thirdparty/dwarf
)

add_subdirectory (thirdparty/dwarf)


##############################################################################
# Set global build options

include (CheckCXXCompilerFlag)

# Adjust warnings
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror=implicit-function-declaration -Werror=missing-prototypes -std=gnu99")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror=missing-prototypes")

# Use GDB extensions
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb -O0")
set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ggdb")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -O0")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ggdb")

# Avoid depending on MinGW runtime DLLs
check_cxx_compiler_flag (-static-libgcc HAVE_STATIC_LIBGCC_FLAG)
if (HAVE_STATIC_LIBGCC_FLAG)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc")
endif ()
check_cxx_compiler_flag (-static-libstdc++ HAVE_STATIC_LIBSTDCXX_FLAG)
if (HAVE_STATIC_LIBSTDCXX_FLAG)
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--kill-at")
endif ()

# Put all executables into top-level bin subdirectory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions (
    # minimum required OS version
    -D_WIN32_WINNT=0x0601
    -DWINVER=0x0601
    # https://msdn.microsoft.com/en-gb/library/windows/desktop/ms683198.aspx
    -DPSAPI_VERSION=1
)


##############################################################################
# Targets

enable_testing ()

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

if (CMAKE_VERSION VERSION_LESS 3.1)
    set (CPACK_GENERATOR "ZIP")
else ()
    set (CPACK_GENERATOR "7Z")
endif ()

include(CPack)
