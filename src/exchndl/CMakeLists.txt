if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_C_COMPILER_ID STREQUAL GNU)
    # Build import library separately, to deal with MinGW issues

    add_library (exchndl MODULE exchndl32exp.def)

    set (EXCHNDL_IMPLIB ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libexchndl.dll.a)
    add_custom_command (
        OUTPUT ${EXCHNDL_IMPLIB}
        COMMAND ${CMAKE_DLLTOOL} --output-lib ${EXCHNDL_IMPLIB} --dllname exchndl.dll --kill-at --input-def ${CMAKE_CURRENT_SOURCE_DIR}/exchndl32imp.def
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exchndl32imp.def
    )
    add_custom_target (libexchndl_implib DEPENDS exchndl ${EXCHNDL_IMPLIB})
    add_library (exchndl_implib INTERFACE IMPORTED GLOBAL)
    target_link_libraries (exchndl_implib INTERFACE ${EXCHNDL_IMPLIB})
    install (FILES ${EXCHNDL_IMPLIB} DESTINATION lib)
else ()
    add_library (exchndl SHARED exchndl64.def)
    add_library (exchndl_implib ALIAS exchndl)
endif ()

target_sources (exchndl PRIVATE
    exchndl.cpp
    version.rc
)

target_link_libraries (exchndl PRIVATE
    common
    mgwhelp_implib
)

set_target_properties (exchndl PROPERTIES
    PREFIX ""
)

install (TARGETS exchndl LIBRARY DESTINATION bin)
