add_executable (catchsegv
    catchsegv.c
)

add_dependencies (catchsegv mgwhelp_implib)

target_link_libraries (catchsegv
    common
    ${MGWHELP_IMPLIB}
)
