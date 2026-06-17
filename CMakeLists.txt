cmake_minimum_required (VERSION 3.28)

cmake_policy (SET CMP0091 NEW)  # Enable CMAKE_MSVC_RUNTIME_LIBRARY
if (POLICY CMP0207)
	cmake_policy (SET CMP0207 NEW)  # file(GET_RUNTIME_DEPENDENCIES) normalizes paths
endif ()


project (drmingw
    VERSION 0.9.13
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
    set(WINNT 0x0A00)
else()
    set(WINNT 0x0601)
endif()

# Put all executables into top-level bin subdirectory
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_compile_definitions (
    UNICODE
    # http://msdn.microsoft.com/en-us/library/aa383745.aspx
    _WIN32_WINNT=${WINNT}
    WINVER=${WINNT}
    # https://msdn.microsoft.com/en-gb/library/windows/desktop/ms683198.aspx
    PSAPI_VERSION=2
    __USE_MINGW_ANSI_STDIO=0

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

set (CPACK_GENERATOR "7Z;NSIS")

set (CPACK_STRIP_FILES ON)

# NSIS installer settings
set (CPACK_NSIS_DISPLAY_NAME "Dr. Mingw")
set (CPACK_NSIS_PACKAGE_NAME "Dr. Mingw")
set (CPACK_NSIS_MANUFACTURER "Jose Fonseca")
set (CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set (CPACK_PACKAGE_INSTALL_DIRECTORY "drmingw-${PROJECT_VERSION}-win64")
set (CPACK_NSIS_CONTACT "jose.fonseca@gmail.com")
set (CPACK_NSIS_URL_INFO_ABOUT "https://github.com/jrfonseca/drmingw")
set (CPACK_NSIS_HELP_LINK "https://github.com/jrfonseca/drmingw")
set (CPACK_NSIS_MENU_LINKS
    "https://github.com/jrfonseca/drmingw" "Dr. Mingw Homepage"
)
set (CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Dr. Mingw.lnk' '$INSTDIR\\\\bin\\\\drmingw.exe'"
)
set (CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\Dr. Mingw.lnk'"
)
# Register drmingw as the default JIT debugger after installation,
# and unregister before uninstallation.
set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS
    "ExecWait '$\\\"$INSTDIR/bin/drmingw.exe$\\\" -i'"
)
set (CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
    "ExecWait '$\\\"$INSTDIR/bin/drmingw.exe$\\\" -u'"
)
set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set (CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/src/drmingw/icon.ico")
set (CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/src/drmingw/icon.ico")

# Embed version info into the installer .exe (visible in File Explorer -> Details).
# CPACK_NSIS_DEFINES is injected into the generated .nsi script.  We build a
# single string with real newlines via configure_file + file(READ) to avoid
# CMake's ";" list-join (NSIS treats ";" as a comment character).
# CPACK_VERBATIM_VARIABLES prevents CPack from re-encoding the value as a list.
file(WRITE "${CMAKE_BINARY_DIR}/NSIS.defines.in"
    "VIProductVersion \"@PROJECT_VERSION_MAJOR@.@PROJECT_VERSION_MINOR@.@PROJECT_VERSION_PATCH@.0\"\n"
    "VIAddVersionKey \"ProductName\" \"Dr. Mingw\"\n"
    "VIAddVersionKey \"CompanyName\" \"Jose Fonseca\"\n"
    "VIAddVersionKey \"FileDescription\" \"Dr. Mingw Installer\"\n"
    "VIAddVersionKey \"FileVersion\" \"@PROJECT_VERSION@\"\n"
    "VIAddVersionKey \"ProductVersion\" \"@PROJECT_VERSION@\"\n"
    "VIAddVersionKey \"LegalCopyright\" \"Jose Fonseca\"\n"
)
configure_file("${CMAKE_BINARY_DIR}/NSIS.defines.in" "${CMAKE_BINARY_DIR}/NSIS.defines" @ONLY)
file(READ "${CMAKE_BINARY_DIR}/NSIS.defines" CPACK_NSIS_DEFINES)
set(CPACK_VERBATIM_VARIABLES TRUE)

set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set (CPACK_NSIS_MODIFY_PATH OFF)

include(CPack)
