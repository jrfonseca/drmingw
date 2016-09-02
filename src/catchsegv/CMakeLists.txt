add_executable (catchsegv
    catchsegv.cpp
)

add_dependencies (catchsegv mgwhelp_implib)

target_link_libraries (catchsegv
    common
    ${MGWHELP_IMPLIB}
    winmm
)

install (TARGETS catchsegv RUNTIME DESTINATION bin)
