add_executable (catchsegv
    catchsegv.cpp
)

include_directories (
    ${CMAKE_SOURCE_DIR}/thirdparty/getoptW
)

set_property (TARGET catchsegv APPEND_STRING PROPERTY LINK_FLAGS " -municode")

add_dependencies (catchsegv mgwhelp_implib)

target_link_libraries (catchsegv
    common
    getoptW
    ${MGWHELP_IMPLIB}
    winmm
)

install (TARGETS catchsegv RUNTIME DESTINATION bin)
