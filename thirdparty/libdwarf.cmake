configure_file (libdwarf/libdwarf/libdwarf.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/support/libdwarf/libdwarf.h
    COPYONLY)

set (HAVE_ALLOCA YES)
set (HAVE_ELF64_RELA YES)
set (HAVE_ELF64_R_INFO YES)
set (HAVE_ELF64_SYM YES)
set (HAVE_ELF_H YES)
set (HAVE_MALLOC_H YES)
set (HAVE_MEMORY_H YES)
set (HAVE_NONSTANDARD_PRINTF_64_FORMAT YES)
set (HAVE_STDINT_H YES)
set (HAVE_STDLIB_H YES)
set (HAVE_STRINGS_H YES)
set (HAVE_STRING_H YES)
set (HAVE_SYS_TYPES_H YES)
set (HAVE_SYS_STAT_H YES)
set (HAVE_UINTPTR_T YES)
set (HAVE_INTPTR_T YES)
set (HAVE_UNISTD_H YES)
set (HAVE_UNUSED_ATTRIBUTE YES)
set (HAVE_WINDOWS_H YES)
set (HAVE_WINDOWS_PATH_H YES)
set (HAVE_ZLIB YES)
set (HAVE_ZLIB_H YES)
set (STDC_HEADERS YES)

configure_file (libdwarf/config.h.in.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/support/libdwarf/config.h)

add_library (dwarf STATIC
    libdwarf/libdwarf/dwarf_abbrev.c
    libdwarf/libdwarf/dwarf_alloc.c
    libdwarf/libdwarf/dwarf_arange.c
    libdwarf/libdwarf/dwarf_die_deliv.c
    libdwarf/libdwarf/dwarf_dsc.c
    libdwarf/libdwarf/dwarf_dnames.c
    libdwarf/libdwarf/dwarf_error.c
    libdwarf/libdwarf/dwarf_form.c
    libdwarf/libdwarf/dwarf_frame.c
    libdwarf/libdwarf/dwarf_frame2.c
    libdwarf/libdwarf/dwarf_funcs.c
    libdwarf/libdwarf/dwarf_global.c
    libdwarf/libdwarf/dwarf_groups.c
    libdwarf/libdwarf/dwarf_harmless.c
    libdwarf/libdwarf/dwarf_init_finish.c
    libdwarf/libdwarf/dwarf_leb.c
    libdwarf/libdwarf/dwarf_line.c
    libdwarf/libdwarf/dwarf_loc.c
    libdwarf/libdwarf/dwarf_macro.c
    libdwarf/libdwarf/dwarf_macro5.c
    libdwarf/libdwarf/dwarf_names.c
    libdwarf/libdwarf/dwarf_print_lines.c
    libdwarf/libdwarf/dwarf_pubtypes.c
    libdwarf/libdwarf/dwarf_query.c
    libdwarf/libdwarf/dwarf_ranges.c
    libdwarf/libdwarf/dwarf_rnglists.c
    libdwarf/libdwarf/dwarfstring.c
    libdwarf/libdwarf/dwarf_stubs.c
    libdwarf/libdwarf/dwarf_tied.c
    libdwarf/libdwarf/dwarf_tsearchhash.c
    libdwarf/libdwarf/dwarf_types.c
    libdwarf/libdwarf/dwarf_util.c
    libdwarf/libdwarf/dwarf_vars.c
    libdwarf/libdwarf/dwarf_weaks.c
    libdwarf/libdwarf/dwarf_xu_index.c
    libdwarf/libdwarf/malloc_check.c
    libdwarf/libdwarf/pro_encode_nm.c
)

target_include_directories (dwarf PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/support/libdwarf
    libdwarf/libdwarf
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
    FILES libdwarf/libdwarf/LIBDWARFCOPYRIGHT
    DESTINATION doc
    RENAME LICENSE-libdwarf.txt
)
