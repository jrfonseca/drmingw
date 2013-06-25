set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -ggdb")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

add_executable (test
    test.c
)
add_dependencies (test mgwhelp_implib)
target_link_libraries (test
    ${MGWHELP_IMPLIB}
)
