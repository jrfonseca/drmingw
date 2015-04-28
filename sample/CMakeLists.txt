set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")

add_executable (sample
    sample.cpp
    exchndl2.cpp
)

install (FILES sample.cpp exchndl2.cpp DESTINATION sample)
install (FILES sample.mak DESTINATION sample RENAME Makefile)
