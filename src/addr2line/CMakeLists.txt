add_executable (addr2line
    addr2line.cpp
)

add_dependencies (addr2line mgwhelp_implib)

target_link_libraries (addr2line
    common
    mgwhelp_implib
)

install (TARGETS addr2line RUNTIME DESTINATION bin)
