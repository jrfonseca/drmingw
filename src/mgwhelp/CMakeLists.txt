if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_C_COMPILER_ID STREQUAL GNU)
    add_library (mgwhelp SHARED mgwhelp32.def)
else ()
    add_library (mgwhelp SHARED mgwhelp64.def)
endif ()

target_sources (mgwhelp PRIVATE
    dwarf_find.cpp
    dwarf_pe.cpp
    mgwhelp.cpp
    version.rc
)

target_link_libraries (mgwhelp PRIVATE
    common
    dwarf
    libiberty
    dbghelp
)

set_target_properties (mgwhelp PROPERTIES
    PREFIX ""
)


add_custom_command (
    TARGET mgwhelp
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DCMAKE_OBJDUMP=${CMAKE_OBJDUMP} -DTARGET=$<TARGET_FILE:mgwhelp> -P ${CMAKE_SOURCE_DIR}/cmake/CheckDependencies.cmake
    VERBATIM
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
