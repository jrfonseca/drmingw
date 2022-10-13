if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_C_COMPILER_ID STREQUAL GNU)
    # Build import library separately, to deal with MinGW issues

    add_library (mgwhelp MODULE mgwhelp32exp.def)

    set (MGWHELP_IMPLIB ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libmgwhelp.dll.a)
    add_custom_command (
        OUTPUT ${MGWHELP_IMPLIB}
        COMMAND ${CMAKE_DLLTOOL} --output-lib ${MGWHELP_IMPLIB} --dllname mgwhelp.dll --kill-at --input-def ${CMAKE_CURRENT_SOURCE_DIR}/mgwhelp32imp.def
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mgwhelp32imp.def
    )
    add_custom_target (libmgwhelp_implib DEPENDS mgwhelp ${MGWHELP_IMPLIB})
    add_library (mgwhelp_implib INTERFACE IMPORTED GLOBAL)
    target_link_libraries (mgwhelp_implib INTERFACE ${MGWHELP_IMPLIB})
    install (FILES ${MGWHELP_IMPLIB} DESTINATION lib)
else ()
    add_library (mgwhelp SHARED mgwhelp64.def)
    add_library (mgwhelp_implib ALIAS mgwhelp)
endif ()

target_sources (mgwhelp PRIVATE
    dwarf_find.cpp
    dwarf_pe.cpp
    mgwhelp.cpp
    version.rc
)

target_link_libraries (mgwhelp LINK_PRIVATE
    common
    dwarf
    libiberty
    dbghelp
)

set_target_properties (mgwhelp PROPERTIES
    PREFIX ""
)

install (TARGETS mgwhelp LIBRARY DESTINATION bin)



# Bundle DbgHelp and SymSrv DLLs
if (WINDBG_FOUND)
    install (
        FILES
            "${WINDBG_DIR}/dbghelp.dll"
            "${WINDBG_DIR}/symsrv.dll"
            "${WINDBG_DIR}/symsrv.yes"
        DESTINATION bin
    )
    if (EXISTS "${WINDBG_DIR}/dbgcore.dll")
        install (
            FILES
                "${WINDBG_DIR}/dbgcore.dll"
            DESTINATION bin
        )
    endif ()
endif ()
