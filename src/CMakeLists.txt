include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_subdirectory (common)

add_subdirectory (mgwhelp)
set (MGWHELP_IMPLIB ${MGWHELP_IMPLIB} PARENT_SCOPE)

add_subdirectory (drmingw)

add_subdirectory (exchndl)

add_subdirectory (addr2line)
