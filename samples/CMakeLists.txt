
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")

add_executable (test
        test.c
        exchndl2.cxx
)

add_executable (testcpp
	testcpp.cxx
	exchndl2.cxx
)

install (FILES test.c testcpp.cxx exchndl2.cxx DESTINATION samples)
install (TARGETS test testcpp RUNTIME DESTINATION samples)
