add_library (dwarfstack
    dwarfstack/src/dwst-exception-dialog.c
    dwarfstack/src/dwst-exception.c
    dwarfstack/src/dwst-file.c
    dwarfstack/src/dwst-location.c
    dwarfstack/src/dwst-process.c
    dwarfstack/mgwhelp/dwarf_pe.c
)

target_compile_definitions (dwarfstack PRIVATE
    DW_TSHASHTYPE=uintptr_t
    LIBDWARF_STATIC
)

target_compile_definitions (dwarfstack
    PRIVATE
        "UNUSEDARG=__attribute__((unused))"
    PUBLIC
        DWST_STATIC
)

target_link_libraries (dwarfstack PRIVATE
    dwarf
    dbghelp
    gdi32
)

target_include_directories (dwarfstack
    PRIVATE
        dwarfstack/mgwhelp
    PUBLIC
        dwarfstack/include
)

install (
    FILES dwarfstack/LICENSE.txt
    DESTINATION doc
    RENAME LICENSE-dwarfstack.txt
)
