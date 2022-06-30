cmake_minimum_required (VERSION 3.16)


project (drmingw)


set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "9")
set (CPACK_PACKAGE_VERSION_PATCH "5")


option (ENABLE_COVERAGE "Enable code coverage." OFF)
option (POSIX_THREADS "Allow posix threads." OFF)


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

# Adjust warnings
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror=implicit-function-declaration -Werror=missing-prototypes")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Disable strict aliasing rules
add_compiler_flags (-fno-strict-aliasing)

include (StaticCRT)

# Avoid Posix threads.  Posix threads is required for support of certain C++11
# multi-threading features, but it introduces a new DLL dependency and we don't
# use those features.
execute_process (
    COMMAND "${CMAKE_COMMAND}" -E echo "#include <thread>\n#ifdef _GLIBCXX_HAS_GTHREADS\n#error _GLIBCXX_HAS_GTHREADS\n#endif"
    COMMAND "${CMAKE_CXX_COMPILER}" -x c++ -E -
    RESULT_VARIABLE STATUS_CXX11_THREADS
    OUTPUT_QUIET
    ERROR_QUIET
)
if (NOT STATUS_CXX11_THREADS EQUAL 0)
    if (POSIX_THREADS)
        message (WARNING "Win32 threads recommended.")
    else ()
        message (SEND_ERROR "Win32 threads required.")
    endif ()
endif ()

# Enable stack protection
# XXX: Broken on https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86832
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0" OR
    CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.3")
    add_compiler_flags (-fstack-protector-all)
    # MinGW doesn't link against libssp automatically, and furthermore
    # we want static linking.
    set (SSP_LIBRARY "-Wl,-Bstatic -lssp -Wl,-Bdynamic")
    set (CMAKE_C_STANDARD_LIBRARIES "${SSP_LIBRARY} ${CMAKE_C_STANDARD_LIBRARIES}")
    set (CMAKE_CXX_STANDARD_LIBRARIES "${SSP_LIBRARY} ${CMAKE_CXX_STANDARD_LIBRARIES}")
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions (-DHAVE_WIN64=1)
else ()
    add_linker_flags (-Wl,--enable-stdcall-fixup)
    add_definitions (-DHAVE_WIN64=0)
endif ()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*|arm64.*|ARM64.*)")
    SET(WINNT 0x0A00)
else()
    SET(WINNT 0x0601)
endif()

# Put all executables into top-level bin subdirectory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_definitions (
    # Require Windows 7
    # http://msdn.microsoft.com/en-us/library/aa383745.aspx
    -D_WIN32_WINNT=${WINNT}
    -DWINVER=${WINNT}
    # https://msdn.microsoft.com/en-gb/library/windows/desktop/ms683198.aspx
    -DPSAPI_VERSION=2

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
add_custom_target (check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    USES_TERMINAL
)

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
