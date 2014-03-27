# Silence warnings on 32bits builds
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast")
endif ()

include_directories (${CMAKE_SOURCE_DIR}/thirdparty/elf)
add_definitions (-DHAVE_ELF_H)

add_library (dwarf STATIC
    dwarf_abbrev.c
    dwarf_addr_finder.c
    dwarf_alloc.c
    dwarf_arange.c
    dwarf_die_deliv.c
    dwarf_error.c
    dwarf_form.c
    dwarf_frame2.c
    dwarf_frame3.c
    dwarf_frame.c
    dwarf_funcs.c
    dwarf_global.c
    dwarf_harmless.c
    dwarf_init_finish.c
    dwarf_leb.c
    dwarf_line2.c
    dwarf_line.c
    dwarf_loc.c
    dwarf_macro.c
    dwarf_print_lines.c
    dwarf_pubtypes.c
    dwarf_query.c
    dwarf_ranges.c
    dwarf_sort_line.c
    dwarf_string.c
    dwarf_stubs.c
    dwarf_types.c
    dwarf_util.c
    dwarf_vars.c
    dwarf_weaks.c
    malloc_check.c
)

