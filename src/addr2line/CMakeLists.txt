add_executable (addr2line
    addr2line.cpp
)

target_link_libraries (addr2line
    common
    mgwhelp_implib
)

install (TARGETS addr2line RUNTIME DESTINATION bin)
