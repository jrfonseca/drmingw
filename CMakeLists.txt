cmake_minimum_required (VERSION 3.16)

# Enable CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy (SET CMP0091 NEW)


project (drmingw
    VERSION 0.9.11
    LANGUAGES C CXX
)


option (ENABLE_COVERAGE "Enable code coverage." OFF)


##############################################################################
# Dependencies

if (NOT MINGW OR CYGWIN)
    message (FATAL_ERROR "MinGW toolchain required")
endif ()

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package (WinDbg)

include_directories (
    ${CMAKE_SOURCE_DIR}/thirdparty/libiberty
)

add_subdirectory (thirdparty)


##############################################################################
# Set global build options

set (CMAKE_C_STANDARD 11)
set (CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_STANDARD 17)
set (CXX_STANDARD_REQUIRED ON)

include (CheckCXXCompilerFlag)

# Adjust warnings
add_compile_options (-Wall "$<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration -Werror=missing-prototypes>")

# Disable strict aliasing rules
add_compile_options (-fno-strict-aliasing)

include (StaticCRT)

# Avoid Posix threads.  Posix threads is required for support of certain C++11
# multi-threading features, but it introduces a new DLL dependency and we don't
# use those features.
# https://github.com/jrfonseca/drmingw/issues/82#issuecomment-1360081041
execute_process (
    COMMAND "${CMAKE_COMMAND}" -E echo "#include <thread>\n#ifdef _GLIBCXX_GCC_GTHR_POSIX_H\n#error _GLIBCXX_GCC_GTHR_POSIX_H\n#endif"
    COMMAND "${CMAKE_CXX_COMPILER}" -x c++ -E -
    RESULT_VARIABLE STATUS_CXX11_THREADS
    OUTPUT_QUIET
    ERROR_QUIET
)
if (NOT STATUS_CXX11_THREADS EQUAL 0)
    message (SEND_ERROR "Win32 threads required.")
endif ()

# Enable stack protection
# XXX: Broken on https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86832
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0" OR
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.3")
    add_compile_options (-fstack-protector-all)
    # MinGW doesn't link against libssp automatically.
    link_libraries (ssp)
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions (HAVE_WIN64=1)
else ()
    add_compile_definitions (HAVE_WIN64=0)
endif ()

# Require Windows 10 for Windows on ARM, Windows 7 for all else
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*|arm64.*|ARM64.*)")
    SET(WINNT 0x0A00)
else()
    SET(WINNT 0x0601)
endif()

# Put all executables into top-level bin subdirectory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_compile_definitions (
    # http://msdn.microsoft.com/en-us/library/aa383745.aspx
    _WIN32_WINNT=${WINNT}
    WINVER=${WINNT}
    # https://msdn.microsoft.com/en-gb/library/windows/desktop/ms683198.aspx
    PSAPI_VERSION=2

    # version
    PACKAGE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PACKAGE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PACKAGE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
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

    # https://github.com/jrfonseca/drmingw/issues/42
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gdwarf-aranges")
    endif ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-aranges")
    endif ()
endmacro ()


##############################################################################
# Targets

enable_testing ()
if (CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    add_custom_target (check)
elseif (DEFINED CMAKE_BUILD_TYPE)
    # Single configuration
    add_custom_target (check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure USES_TERMINAL)
else ()
    # Multiple configuration
    add_custom_target (check COMMAND ${CMAKE_CTEST_COMMAND} -C "$<CONFIG>" --output-on-failure USES_TERMINAL)
endif ()

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
install (FILES include/exchndl.h DESTINATION include)

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

if (CMAKE_SYSTEM_PROCESSOR STREQUAL ARM)
    set (CPACK_SYSTEM_NAME win32-arm)
endif ()
if (CMAKE_SYSTEM_PROCESSOR STREQUAL ARM64)
    set (CPACK_SYSTEM_NAME win64-arm)
endif ()

set (CPACK_GENERATOR "7Z")

set (CPACK_STRIP_FILES ON)

include(CPack)
