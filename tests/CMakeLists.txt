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
)
add_test (
    NAME test_mgwhelp
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:mgwhelp_test>
)

#
# test_mgwhelp_stripped
#

# https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test.debug
    COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:mgwhelp_test> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test.debug
    DEPENDS mgwhelp_test
    VERBATIM
)
add_custom_command (
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:mgwhelp_test> --add-gnu-debuglink=mgwhelp_stripped_test.debug ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test.debug
    VERBATIM
)
add_custom_target (mgwhelp_stripped_test ALL
    DEPENDS
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test.debug
)
add_test (
    NAME test_mgwhelp_stripped
    COMMAND ${WINE_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
)


#
# test_exchndl
#

add_executable (exchndl_test
    exchndl_test.c
)
add_dependencies (exchndl_test exchndl)
add_test (
    NAME test_exchndl
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:exchndl_test>
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)


#
# test_addr2line
#

add_executable (addr2line_test
    addr2line_test.c
)
add_dependencies (addr2line_test addr2line)
add_test (
    NAME test_addr2line
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:addr2line_test>
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)


#
# test_catchsegv
#

add_subdirectory (apps)
find_package (PythonInterp 2.7)
if (PYTHONINTERP_FOUND)
    add_test (
        NAME test_catchsegv
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/apps/test.py $<TARGET_FILE:catchsegv> ${CMAKE_CURRENT_BINARY_DIR}/apps
    )
endif ()
