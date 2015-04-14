if (CMAKE_CROSSCOMPILING)
    find_program (WINE_PROGRAM wine)
    math (EXPR ARCH_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
    set (WINE_COMMAND env "WINEDEBUG=+debugstr" "WINEARCH=win${ARCH_BITS}" "${WINE_PROGRAM}")
else ()
    set (WINE_COMMAND)
endif ()


set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -ggdb")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")


add_executable (mgwhelp_test
    mgwhelp_test.c
)
add_dependencies (mgwhelp_test mgwhelp_implib)
target_link_libraries (mgwhelp_test
    ${MGWHELP_IMPLIB}
)
add_custom_target (test_mgwhelp
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:mgwhelp_test>
    DEPENDS mgwhelp_test mgwhelp
    VERBATIM
)


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
add_custom_target (test_mgwhelp_stripped
    COMMAND ${WINE_COMMAND} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mgwhelp_stripped_test${CMAKE_EXECUTABLE_SUFFIX}
    DEPENDS mgwhelp_stripped_test mgwhelp
    VERBATIM
)


add_executable (exchndl_test
    exchndl_test.c
)
add_custom_target (test_exchndl
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:exchndl_test>
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    DEPENDS exchndl_test exchndl mgwhelp
    VERBATIM
)


add_subdirectory (apps)


if (POLICY CMP0037)
    cmake_policy (SET CMP0037 OLD)
endif ()

add_custom_target (test
    DEPENDS
        test_mgwhelp
        test_mgwhelp_stripped
        test_exchndl
)
