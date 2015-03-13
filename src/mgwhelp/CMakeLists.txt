if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (MGWHELP_DEF mgwhelp32.def)
else ()
    set (MGWHELP_DEF mgwhelp64.def)
endif ()

add_library (mgwhelp SHARED
    ${MGWHELP_DEF}
    demangle.cxx
    dwarf_pe.c
    mgwhelp.c
)

target_link_libraries (mgwhelp LINK_PRIVATE
    common
    dwarf
    dbghelp
)

set_target_properties (mgwhelp PROPERTIES
    PREFIX ""
)

if (BINUTILS_FOUND)
    set_target_properties (mgwhelp PROPERTIES
	COMPILE_DEFINITIONS HAVE_BFD=1
    )
    target_link_libraries (mgwhelp LINK_PRIVATE
        ${BINUTILS_BFD_LIBRARY}
        ${BINUTILS_LIBERTY_LIBRARY}
    )
endif ()

install (TARGETS mgwhelp RUNTIME DESTINATION bin)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    # XXX: We need to use a custom import lib to avoid link failures
    string (REPLACE gcc dlltool DLLTOOL ${CMAKE_C_COMPILER})
    set (MGWHELP_IMPLIB ${CMAKE_CURRENT_BINARY_DIR}/libmgwhelp.a)
    add_custom_command (
        OUTPUT ${MGWHELP_IMPLIB}
	COMMAND ${DLLTOOL} --output-lib ${MGWHELP_IMPLIB} --dllname mgwhelp.dll --kill-at --input-def=${CMAKE_CURRENT_SOURCE_DIR}/dbghelp.def
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dbghelp.def
    )
else ()
    set (MGWHELP_IMPLIB mgwhelp)
endif ()
set (MGWHELP_IMPLIB ${MGWHELP_IMPLIB} PARENT_SCOPE)

add_custom_target (mgwhelp_implib DEPENDS ${MGWHELP_IMPLIB})


# Bundle DbgHelp and SymSrv DLLs
if (WINDBG_FOUND)
    install (
        FILES
            "${WINDBG_DIR}/dbghelp.dll"
            "${WINDBG_DIR}/symsrv.dll"
            "${WINDBG_DIR}/symsrv.yes"
        DESTINATION bin
    )
endif ()
