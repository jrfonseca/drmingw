set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -ggdb")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

add_executable (sample
    sample.cpp
    exchndl2.cpp
)

install (FILES sample.cpp exchndl2.cpp DESTINATION sample)
install (FILES sample.mak DESTINATION sample RENAME Makefile)
