add_executable (addr2line
    addr2line.cpp
)

include_directories (
    ${CMAKE_SOURCE_DIR}/thirdparty/getoptW
)

set_property (TARGET addr2line APPEND_STRING PROPERTY LINK_FLAGS " -municode")

target_link_libraries (addr2line PRIVATE
    common
    getoptW
    mgwhelp
)

install (TARGETS addr2line RUNTIME DESTINATION bin)
