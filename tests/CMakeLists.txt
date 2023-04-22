force_debug ()

#
# test_mgwhelp
#

add_executable (test_mgwhelp
    test_mgwhelp.cpp
)
target_link_libraries (test_mgwhelp
    mgwhelp
    shlwapi
)
add_test (
    NAME test_mgwhelp
    COMMAND test_mgwhelp
)
set_tests_properties(test_mgwhelp PROPERTIES FIXTURES_REQUIRED test_fixture)

#
# test_mgwhelp_split
#
# https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
#
# Use --strip-all to also strip the PE symbol table.
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
    DEPENDS test_mgwhelp
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.exe
    COMMAND ${CMAKE_OBJCOPY} --strip-all $<TARGET_FILE:test_mgwhelp> --add-gnu-debuglink=test_mgwhelp_split.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.exe
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
    VERBATIM
)
add_custom_target (test_mgwhelp_split ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.exe
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
)
add_test (
    NAME test_mgwhelp_split
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.exe
)
set_tests_properties(test_mgwhelp_split PROPERTIES FIXTURES_REQUIRED test_fixture)


#
# test_mgwhelp_split_subdir
#
# Same as test_mgwhelp_split, but on a .debug subdir
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/test_mgwhelp_split_subdir.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/test_mgwhelp_split_subdir.debug
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS test_mgwhelp ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir.exe
    COMMAND ${CMAKE_OBJCOPY} --strip-all $<TARGET_FILE:test_mgwhelp> --add-gnu-debuglink=test_mgwhelp_split_subdir.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir.exe
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/test_mgwhelp_split_subdir.debug
    VERBATIM
)
add_custom_target (test_mgwhelp_split_subdir ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir.exe
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/test_mgwhelp_split_subdir.debug
)
add_test (
    NAME test_mgwhelp_split_subdir
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir.exe
)
set_tests_properties(test_mgwhelp_split_subdir PROPERTIES FIXTURES_REQUIRED test_fixture)

#
# test_mgwhelp_stripped
#
# Use --strip-debug instad of --strip-all to keep PE symbol table.
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped.exe
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped.exe
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS test_mgwhelp
    VERBATIM
)
add_custom_target (test_mgwhelp_stripped ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped.exe
)
add_test (
    NAME test_mgwhelp_stripped
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped.exe
)
set_tests_properties(test_mgwhelp_stripped PROPERTIES FIXTURES_REQUIRED test_fixture)


#
# test_mgwhelp_zdebug
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug --compress-debug-sections $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
    DEPENDS test_mgwhelp
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.exe
    COMMAND ${CMAKE_OBJCOPY} --strip-all $<TARGET_FILE:test_mgwhelp> --add-gnu-debuglink=test_mgwhelp_zdebug.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.exe
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
    VERBATIM
)
add_custom_target (test_mgwhelp_zdebug ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.exe
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
)
add_test (
    NAME test_mgwhelp_zdebug
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.exe
)

# XXX: MinGW support for objcopy --compress-debug-sections flag is very flaky,
# depending on the exact toolchain:
#
# - some (e.g, the x86_64-4.9.2-release-win32-seh-rt_v4-rev4 used in Appveyor)
#   just works
#
# - others (e.g the ppa:tobydox/mingw-x-trusty used in Travis) fail when
#   compressing the debug sections with:
#
#     BFD: test_mgwhelp.exe: unable to initialize compress status for section .debug_aranges
#
# - others (e.g, Ubuntu 15.10's) appear to work but cause libdwarf to fail with
#   DW_DLE_ARANGE_OFFSET_BAD
#
# See also:
# - https://bugs.kde.org/show_bug.cgi?id=303877
# - https://sourceware.org/bugzilla/show_bug.cgi?id=14067
# - https://bugzilla.redhat.com/show_bug.cgi?id=679777
#
if (NOT DEFINED OBJCOPY_VERSION)
    execute_process (
        COMMAND ${CMAKE_OBJCOPY} --version
        OUTPUT_VARIABLE OBJCOPY_VERSION_OUTPUT
        RESULT_VARIABLE OBJCOPY_VERSION_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (NOT ${OBJCOPY_VERSION_RESULT} EQUAL 0)
        message (SEND_ERROR "Command \"${CMAKE_OBJCOPY} --version\" failed")
    else ()
        # e.g., "GNU objcopy (GNU Binutils) 2.34"
        if ("${OBJCOPY_VERSION_OUTPUT}" MATCHES "^GNU objcopy \\([^)]+\\) ([^\n]+)")
            set (OBJCOPY_VERSION "${CMAKE_MATCH_1}" CACHE STRING "" FORCE)
            message (STATUS "objcopy version: ${OBJCOPY_VERSION}")
        elseif ("${OBJCOPY_VERSION_OUTPUT}" MATCHES "^llvm-objcopy, compatible with GNU objcopy")
            # --compress-debug-sections does not work if LLVM was compiled
            # without LLVM_ENABLE_ZLIB
            set (OBJCOPY_VERSION "llvm-objcopy" CACHE STRING "" FORCE)
            message (STATUS "objcopy version: llvm-objcopy")
        else ()
            message (SEND_ERROR "Failed to parse ${CMAKE_OBJCOPY} version")
        endif ()
    endif ()
endif ()
if (OBJCOPY_VERSION STREQUAL "llvm-objcopy")
    set_target_properties (test_mgwhelp_zdebug PROPERTIES EXCLUDE_FROM_ALL ON)
else ()
    set_tests_properties(test_mgwhelp_zdebug PROPERTIES FIXTURES_REQUIRED test_fixture)
endif ()
if (OBJCOPY_VERSION STREQUAL "llvm-objcopy" OR OBJCOPY_VERSION VERSION_LESS "2.34")
    set_tests_properties (test_mgwhelp_zdebug PROPERTIES DISABLED ON)
endif ()


#
# test_exchndl_static_unicode
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (test_exchndl_static_unicode
    test_exchndl_static_unicode.cpp
)
target_link_libraries (test_exchndl_static_unicode exchndl shlwapi)
add_test (
    NAME test_exchndl_static_unicode
    COMMAND test_exchndl_static_unicode
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set_tests_properties(test_exchndl_static_unicode PROPERTIES FIXTURES_REQUIRED test_fixture)


#
# test_exchndl_static_ansi
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (test_exchndl_static_ansi
    test_exchndl_static_ansi.cpp
)
target_link_libraries (test_exchndl_static_ansi exchndl shlwapi)
add_test (
    NAME test_exchndl_static_ansi
    COMMAND test_exchndl_static_ansi
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set_tests_properties(test_exchndl_static_ansi PROPERTIES FIXTURES_REQUIRED test_fixture)


#
# test_exchndl_dynamic_unicode
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (test_exchndl_dynamic_unicode
    test_exchndl_dynamic_unicode.cpp
)
target_link_libraries (test_exchndl_dynamic_unicode shlwapi)
add_test (
    NAME test_exchndl_dynamic_unicode
    COMMAND test_exchndl_dynamic_unicode
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set_tests_properties(test_exchndl_dynamic_unicode PROPERTIES FIXTURES_REQUIRED test_fixture)


#
# test_exchndl_dynamic_ansi
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (test_exchndl_dynamic_ansi
    test_exchndl_dynamic_ansi.cpp
)
target_link_libraries (test_exchndl_dynamic_ansi shlwapi)
add_test (
    NAME test_exchndl_dynamic_ansi
    COMMAND test_exchndl_dynamic_ansi
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set_tests_properties(test_exchndl_dynamic_ansi PROPERTIES FIXTURES_REQUIRED test_fixture)


#
# test_addr2line
#

add_executable (test_addr2line
    test_addr2line.cpp
)
add_test (
    NAME test_addr2line
    COMMAND test_addr2line
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set_tests_properties(test_addr2line PROPERTIES FIXTURES_REQUIRED test_fixture)

add_test(test_build
    "${CMAKE_COMMAND}"
    --build "${CMAKE_BINARY_DIR}"
    --config "$<CONFIG>"
    --target all
)

set_tests_properties(test_build PROPERTIES FIXTURES_SETUP test_fixture)
