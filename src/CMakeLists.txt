if (ENABLE_COVERAGE)
    add_compile_options (--coverage)
    add_link_options (--coverage)
endif ()

include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_subdirectory (common)
add_subdirectory (mgwhelp)
add_subdirectory (drmingw)
add_subdirectory (exchndl)
add_subdirectory (addr2line)
add_subdirectory (catchsegv)
