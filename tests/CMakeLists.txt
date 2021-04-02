if (CMAKE_CROSSCOMPILING)
    find_program (WINE_PROGRAM wine)
    set (WINE_COMMAND env "WINEDEBUG=+debugstr" "${WINE_PROGRAM}")
else ()
    set (WINE_COMMAND)
endif ()


force_debug ()


#
# test_mgwhelp
#

add_executable (mgwhelp_test
    mgwhelp_test.cpp
)
add_dependencies (mgwhelp_test mgwhelp_implib)
target_link_libraries (mgwhelp_test
    ${MGWHELP_IMPLIB}
    shlwapi
)
add_dependencies (check mgwhelp_test)
add_test (
    NAME test_mgwhelp
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:mgwhelp_test>
)

#
# test_mgwhelp_split
#
# https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:mgwhelp_test> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test.debug
    DEPENDS mgwhelp_test
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:mgwhelp_test> --add-gnu-debuglink=mgwhelp_split_test.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test.debug
    VERBATIM
)
add_custom_target (mgwhelp_split_test ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test${CMAKE_EXECUTABLE_SUFFIX}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test.debug
)
add_dependencies (check mgwhelp_split_test)
add_test (
    NAME test_mgwhelp_split
    COMMAND ${WINE_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_test${CMAKE_EXECUTABLE_SUFFIX}
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
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/mgwhelp_split_subdir_test.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:mgwhelp_test> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/mgwhelp_split_subdir_test.debug
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS mgwhelp_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_subdir_test${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:mgwhelp_test> --add-gnu-debuglink=mgwhelp_split_subdir_test.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_subdir_test${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/mgwhelp_split_subdir_test.debug
    VERBATIM
)
add_custom_target (mgwhelp_split_subdir_test ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_subdir_test${CMAKE_EXECUTABLE_SUFFIX}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/.debug/mgwhelp_split_subdir_test.debug
)
add_dependencies (check mgwhelp_split_subdir_test)
add_test (
    NAME test_mgwhelp_split_subdir
    COMMAND ${WINE_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_split_subdir_test${CMAKE_EXECUTABLE_SUFFIX}
)


#
# test_mgwhelp_stripped
#

add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:mgwhelp_test> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS mgwhelp_test
    VERBATIM
)
add_custom_target (mgwhelp_stripped_test ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
)
add_dependencies (check mgwhelp_stripped_test)
add_test (
    NAME test_mgwhelp_stripped
    COMMAND ${WINE_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
)


#
# test_mgwhelp_zdebug
#
# XXX: MinGW support for objcopy --compress-debug-sections flag is very flaky,
# depending on the exact toolchain:
#
# - some (e.g, the x86_64-4.9.2-release-win32-seh-rt_v4-rev4 used in Appveyor)
#   just works
#
# - others (e.g the ppa:tobydox/mingw-x-trusty used in Travis) fail when
#   compressing the debug sections with:
#
#     BFD: mgwhelp_test.exe: unable to initialize compress status for section .debug_aranges
#
# - others (e.g, Ubuntu 15.10's) appear to work but cause libdwarf to fail with
#   DW_DLE_ARANGE_OFFSET_BAD
#
# See also:
# - https://bugs.kde.org/show_bug.cgi?id=303877
# - https://sourceware.org/bugzilla/show_bug.cgi?id=14067
# - https://bugzilla.redhat.com/show_bug.cgi?id=679777
#

option (ENABLE_ZDEBUG_TEST "Enable compressed debug sections test." OFF)
if (ENABLE_ZDEBUG_TEST)
    add_custom_command (
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test.debug
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug --compress-debug-sections $<TARGET_FILE:mgwhelp_test> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test.debug
        DEPENDS mgwhelp_test
        VERBATIM
    )
    add_custom_command (
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test${CMAKE_EXECUTABLE_SUFFIX}
        COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:mgwhelp_test> --add-gnu-debuglink=mgwhelp_zdebug_test.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test${CMAKE_EXECUTABLE_SUFFIX}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test.debug
        VERBATIM
    )
    add_custom_target (mgwhelp_zdebug_test ALL
        DEPENDS
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test${CMAKE_EXECUTABLE_SUFFIX}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test.debug
    )
    add_dependencies (check mgwhelp_zdebug_test)
    add_test (
        NAME test_mgwhelp_zdebug
        COMMAND ${WINE_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_zdebug_test${CMAKE_EXECUTABLE_SUFFIX}
    )
endif ()


#
# test_exchndl
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (exchndl_static_test
    exchndl_static_test.c
)
add_dependencies (exchndl_static_test exchndl_implib)
target_link_libraries (exchndl_static_test ${EXCHNDL_IMPLIB} shlwapi)
add_dependencies (check exchndl_static_test)
add_test (
    NAME test_exchndl_static
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:exchndl_static_test>
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)


#
# test_exchndl
#

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/apps
)
add_executable (exchndl_dynamic_test
    exchndl_dynamic_test.c
)
target_link_libraries (exchndl_dynamic_test shlwapi)
add_dependencies (exchndl_dynamic_test exchndl)
add_dependencies (check exchndl_dynamic_test)
add_test (
    NAME test_exchndl_dynamic
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:exchndl_dynamic_test>
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)


#
# test_addr2line
#

add_executable (addr2line_test
    addr2line_test.c
)
add_dependencies (addr2line_test addr2line)
add_dependencies (check addr2line_test)
add_test (
    NAME test_addr2line
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:addr2line_test>
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
