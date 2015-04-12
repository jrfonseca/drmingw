add_executable (addr2line
    addr2line.c
)

add_dependencies (addr2line mgwhelp_implib)

target_link_libraries (addr2line
    common
    ${MGWHELP_IMPLIB}
)
