add_executable (catchsegv
    catchsegv.cpp
)

include_directories (
    ${CMAKE_SOURCE_DIR}/thirdparty/getoptW
)

set_property (TARGET catchsegv APPEND_STRING PROPERTY LINK_FLAGS " -municode")

target_link_libraries (catchsegv PRIVATE
    common
    getoptW
    mgwhelp
    winmm
)


add_custom_command (
    TARGET catchsegv
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP} -DTARGET=$<TARGET_FILE:catchsegv> -P ${CMAKE_SOURCE_DIR}/cmake/CheckDependencies.cmake
    VERBATIM
)


install (TARGETS catchsegv RUNTIME DESTINATION bin)
