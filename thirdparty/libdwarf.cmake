add_library (dwarf STATIC
    dwarf/dwarf_abbrev.c
    dwarf/dwarf_alloc.c
    dwarf/dwarf_arange.c
    dwarf/dwarf_die_deliv.c
    dwarf/dwarf_dsc.c
    dwarf/dwarf_dnames.c
    dwarf/dwarf_error.c
    dwarf/dwarf_form.c
    dwarf/dwarf_frame.c
    dwarf/dwarf_frame2.c
    dwarf/dwarf_funcs.c
    dwarf/dwarf_global.c
    dwarf/dwarf_groups.c
    dwarf/dwarf_harmless.c
    dwarf/dwarf_init_finish.c
    dwarf/dwarf_leb.c
    dwarf/dwarf_line.c
    dwarf/dwarf_loc.c
    dwarf/dwarf_macro.c
    dwarf/dwarf_macro5.c
    dwarf/dwarf_names.c
    dwarf/dwarf_print_lines.c
    dwarf/dwarf_pubtypes.c
    dwarf/dwarf_query.c
    dwarf/dwarf_ranges.c
    dwarf/dwarf_rnglists.c
    dwarf/dwarfstring.c
    dwarf/dwarf_stubs.c
    dwarf/dwarf_tied.c
    dwarf/dwarf_tsearchhash.c
    dwarf/dwarf_types.c
    dwarf/dwarf_util.c
    dwarf/dwarf_vars.c
    dwarf/dwarf_weaks.c
    dwarf/dwarf_xu_index.c
    dwarf/malloc_check.c
    dwarf/pro_encode_nm.c
)

target_include_directories (dwarf PUBLIC
    support/libdwarf
    dwarf
)

target_compile_definitions (dwarf PRIVATE
    "PACKAGE_VERSION=\"drmingw\""
)

target_compile_options (dwarf PRIVATE
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
)

target_link_libraries (dwarf z)

install (
    FILES dwarf/LIBDWARFCOPYRIGHT
    DESTINATION doc
    RENAME LICENSE-libdwarf.txt
)
