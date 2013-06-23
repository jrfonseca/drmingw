
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -ggdb")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

add_executable (test
        test.c
        exchndl2.cxx
)

add_executable (testcpp
    testcpp.cxx
    exchndl2.cxx
)

add_executable (test-segv
    test-segv.cxx
    exchndl2.cxx
)

add_executable (example
    example.c
)
add_dependencies (example mgwhelp_implib)
target_link_libraries (example
    ${MGWHELP_IMPLIB}
)

install (FILES test.c testcpp.cxx exchndl2.cxx DESTINATION samples)
install (FILES samples.mak DESTINATION samples RENAME Makefile)
install (TARGETS test testcpp RUNTIME DESTINATION samples)
