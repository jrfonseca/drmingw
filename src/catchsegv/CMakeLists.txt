add_executable (catchsegv
    catchsegv.cpp
    getoptW.c
)

set_property (TARGET catchsegv APPEND_STRING PROPERTY LINK_FLAGS " -municode")

add_dependencies (catchsegv mgwhelp_implib)

target_link_libraries (catchsegv
    common
    ${MGWHELP_IMPLIB}
    winmm
)

install (TARGETS catchsegv RUNTIME DESTINATION bin)
