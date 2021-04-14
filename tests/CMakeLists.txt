force_debug ()


#
# test_mgwhelp
#

add_executable (test_mgwhelp
    test_mgwhelp.cpp
)
add_dependencies (test_mgwhelp mgwhelp_implib)
target_link_libraries (test_mgwhelp
    ${MGWHELP_IMPLIB}
    shlwapi
)
add_dependencies (check test_mgwhelp)
add_test (
    NAME test_mgwhelp
    COMMAND test_mgwhelp
)

#
# test_mgwhelp_split
#
# https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
    DEPENDS test_mgwhelp
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:test_mgwhelp> --add-gnu-debuglink=test_mgwhelp_split.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
    VERBATIM
)
add_custom_target (test_mgwhelp_split ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split${CMAKE_EXECUTABLE_SUFFIX}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split.debug
)
add_dependencies (check test_mgwhelp_split)
add_test (
    NAME test_mgwhelp_split
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split${CMAKE_EXECUTABLE_SUFFIX}
)


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
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:test_mgwhelp> --add-gnu-debuglink=test_mgwhelp_split_subdir.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/test_mgwhelp_split_subdir.debug
    VERBATIM
)
add_custom_target (test_mgwhelp_split_subdir ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir${CMAKE_EXECUTABLE_SUFFIX}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/test_mgwhelp_split_subdir.debug
)
add_dependencies (check test_mgwhelp_split_subdir)
add_test (
    NAME test_mgwhelp_split_subdir
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_split_subdir${CMAKE_EXECUTABLE_SUFFIX}
)


#
# test_mgwhelp_stripped
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS test_mgwhelp
    VERBATIM
)
add_custom_target (test_mgwhelp_stripped ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped${CMAKE_EXECUTABLE_SUFFIX}
)
add_dependencies (check test_mgwhelp_stripped)
add_test (
    NAME test_mgwhelp_stripped
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_stripped${CMAKE_EXECUTABLE_SUFFIX}
)


#
# test_mgwhelp_zdebug
#

option (ENABLE_ZDEBUG_TEST "Enable compressed debug sections test." OFF)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug --compress-debug-sections $<TARGET_FILE:test_mgwhelp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
    DEPENDS test_mgwhelp
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:test_mgwhelp> --add-gnu-debuglink=test_mgwhelp_zdebug.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
    VERBATIM
)
add_custom_target (test_mgwhelp_zdebug ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug${CMAKE_EXECUTABLE_SUFFIX}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug.debug
)
add_dependencies (check test_mgwhelp_zdebug)
add_test (
    NAME test_mgwhelp_zdebug
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_mgwhelp_zdebug${CMAKE_EXECUTABLE_SUFFIX}
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
        if ("${OBJCOPY_VERSION_OUTPUT}" MATCHES "^GNU objcopy \\(GNU Binutils\\) ([^\n]+)")
            set (OBJCOPY_VERSION "${CMAKE_MATCH_1}" CACHE STRING "" FORCE)
            message (STATUS "objcopy version: ${OBJCOPY_VERSION}")
        else ()
            message (SEND_ERROR "Failed to parse ${CMAKE_OBJCOPY} version")
        endif ()
    endif ()
endif ()
if (NOT OBJCOPY_VERSION VERSION_GREATER_EQUAL "2.34")
    set_tests_properties (test_mgwhelp_zdebug PROPERTIES DISABLED ON)
endif ()


#
# test_exchndl
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (test_exchndl_static
    test_exchndl_static.cpp
)
add_dependencies (test_exchndl_static exchndl_implib)
target_link_libraries (test_exchndl_static ${EXCHNDL_IMPLIB} shlwapi)
add_dependencies (check test_exchndl_static)
add_test (
    NAME test_exchndl_static
    COMMAND test_exchndl_static
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)


#
# test_exchndl
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (test_exchndl_dynamic
    test_exchndl_dynamic.cpp
)
target_link_libraries (test_exchndl_dynamic shlwapi)
add_dependencies (test_exchndl_dynamic exchndl)
add_dependencies (check test_exchndl_dynamic)
add_test (
    NAME test_exchndl_dynamic
    COMMAND test_exchndl_dynamic
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)


#
# test_addr2line
#

add_executable (test_addr2line
    test_addr2line.cpp
)
add_dependencies (test_addr2line addr2line)
add_dependencies (check test_addr2line)
add_test (
    NAME test_addr2line
    COMMAND test_addr2line
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
