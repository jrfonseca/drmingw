set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -ggdb")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

add_executable (mgwhelp_test
    mgwhelp_test.c
)
add_dependencies (mgwhelp_test mgwhelp_implib)
target_link_libraries (mgwhelp_test
    ${MGWHELP_IMPLIB}
)

if (CMAKE_CROSSCOMPILING)
    find_program (WINE_PROGRAM wine)
    math (EXPR ARCH_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
    set (WINE_COMMAND env "WINEARCH=win${ARCH_BITS}" "${WINE_PROGRAM}")
else ()
    set (WINE_COMMAND)
endif ()

add_custom_target (test_mgwhelp
    COMMAND ${WINE_COMMAND} $<TARGET_FILE:mgwhelp_test>
    DEPENDS mgwhelp_test mgwhelp
    VERBATIM
)

if (POLICY CMP0037)
    cmake_policy (SET CMP0037 OLD)
endif ()
add_custom_target (test DEPENDS test_mgwhelp)
