set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast")

add_library (dwarf STATIC
    dwarf_abbrev.c
    dwarf_alloc.c
    dwarf_arange.c
    dwarf_die_deliv.c
    dwarf_dsc.c
    dwarf_dnames.c
    dwarf_error.c
    dwarf_form.c
    dwarf_frame.c
    dwarf_frame2.c
    dwarf_funcs.c
    dwarf_global.c
    dwarf_groups.c
    dwarf_harmless.c
    dwarf_init_finish.c
    dwarf_leb.c
    dwarf_line.c
    dwarf_loc.c
    dwarf_macro.c
    dwarf_macro5.c
    dwarf_print_lines.c
    dwarf_pubtypes.c
    dwarf_query.c
    dwarf_ranges.c
    dwarf_string.c
    dwarf_stubs.c
    dwarf_tied.c
    dwarf_tsearchhash.c
    dwarf_types.c
    dwarf_util.c
    dwarf_vars.c
    dwarf_weaks.c
    dwarf_xu_index.c
    malloc_check.c
    pro_encode_nm.c
)

target_link_libraries (dwarf z)

install (
    FILES LIBDWARFCOPYRIGHT
    DESTINATION doc
    RENAME LICENSE-libdwarf.txt
)
