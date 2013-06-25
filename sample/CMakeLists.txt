set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -ggdb")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

add_executable (sample
    sample.cxx
    exchndl2.cxx
)

install (FILES sample.cxx exchndl2.cxx DESTINATION sample)
install (FILES sample.mak DESTINATION sample RENAME Makefile)
