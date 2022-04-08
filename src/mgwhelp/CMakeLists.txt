if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (MGWHELP_EXP_DEF mgwhelp32exp.def)
    set (MGWHELP_IMP_DEF mgwhelp32imp.def)
else ()
    set (MGWHELP_EXP_DEF mgwhelp64.def)
    set (MGWHELP_IMP_DEF mgwhelp64.def)
endif ()

add_library (mgwhelp MODULE
    ${MGWHELP_EXP_DEF}
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

add_custom_command (
    OUTPUT ${MGWHELP_IMPLIB}
    COMMAND ${CMAKE_DLLTOOL} --output-lib ${MGWHELP_IMPLIB} --dllname mgwhelp.dll --kill-at --input-def ${CMAKE_CURRENT_SOURCE_DIR}/${MGWHELP_IMP_DEF}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MGWHELP_IMP_DEF}
)
add_custom_target (mgwhelp_implib DEPENDS mgwhelp ${MGWHELP_IMPLIB})
install (FILES ${MGWHELP_IMPLIB} DESTINATION lib)


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
