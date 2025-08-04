set (HAVE_MALLOC_H YES)
set (HAVE_FCNTL_H YES)
set (HAVE_MEMORY_H YES)
set (HAVE_NONSTANDARD_PRINTF_64_FORMAT YES)
set (HAVE_STDINT_H YES)
set (HAVE_INTTYPES_H YES)
set (HAVE_STRINGS_H YES)
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
set (HAVE_ZSTD YES)
set (HAVE_ZSTD_H YES)
set (STDC_HEADERS YES)

configure_file (libdwarf/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/support/libdwarf/config.h)

# See libdwarf/src/lib/libdwarf/CMakeLists.txt
add_library (dwarf STATIC
    libdwarf/src/lib/libdwarf/dwarf_abbrev.c
    libdwarf/src/lib/libdwarf/dwarf_alloc.c
    libdwarf/src/lib/libdwarf/dwarf_arange.c
    libdwarf/src/lib/libdwarf/dwarf_crc.c
    libdwarf/src/lib/libdwarf/dwarf_crc32.c
    libdwarf/src/lib/libdwarf/dwarf_debug_sup.c
    libdwarf/src/lib/libdwarf/dwarf_debugaddr.c
    libdwarf/src/lib/libdwarf/dwarf_debuglink.c
    libdwarf/src/lib/libdwarf/dwarf_debugnames.c
    libdwarf/src/lib/libdwarf/dwarf_die_deliv.c
    libdwarf/src/lib/libdwarf/dwarf_dsc.c
    libdwarf/src/lib/libdwarf/dwarf_elf_load_headers.c
    libdwarf/src/lib/libdwarf/dwarf_elf_rel_detector.c
    libdwarf/src/lib/libdwarf/dwarf_elfread.c
    libdwarf/src/lib/libdwarf/dwarf_error.c
    libdwarf/src/lib/libdwarf/dwarf_fill_in_attr_form.c
    libdwarf/src/lib/libdwarf/dwarf_find_sigref.c
    libdwarf/src/lib/libdwarf/dwarf_fission_to_cu.c
    libdwarf/src/lib/libdwarf/dwarf_form.c
    libdwarf/src/lib/libdwarf/dwarf_form_class_names.c
    libdwarf/src/lib/libdwarf/dwarf_frame.c
    libdwarf/src/lib/libdwarf/dwarf_frame2.c
    libdwarf/src/lib/libdwarf/dwarf_gdbindex.c
    libdwarf/src/lib/libdwarf/dwarf_generic_init.c
    libdwarf/src/lib/libdwarf/dwarf_global.c
    libdwarf/src/lib/libdwarf/dwarf_gnu_index.c
    libdwarf/src/lib/libdwarf/dwarf_groups.c
    libdwarf/src/lib/libdwarf/dwarf_harmless.c
    libdwarf/src/lib/libdwarf/dwarf_init_finish.c
    libdwarf/src/lib/libdwarf/dwarf_leb.c
    libdwarf/src/lib/libdwarf/dwarf_line.c
    libdwarf/src/lib/libdwarf/dwarf_loc.c
    libdwarf/src/lib/libdwarf/dwarf_local_malloc.c
    libdwarf/src/lib/libdwarf/dwarf_locationop_read.c
    libdwarf/src/lib/libdwarf/dwarf_loclists.c
    libdwarf/src/lib/libdwarf/dwarf_machoread.c
    libdwarf/src/lib/libdwarf/dwarf_macro.c
    libdwarf/src/lib/libdwarf/dwarf_macro5.c
    libdwarf/src/lib/libdwarf/dwarf_memcpy_swap.c
    libdwarf/src/lib/libdwarf/dwarf_names.c
    libdwarf/src/lib/libdwarf/dwarf_object_detector.c
    libdwarf/src/lib/libdwarf/dwarf_object_read_common.c
    libdwarf/src/lib/libdwarf/dwarf_peread.c
    libdwarf/src/lib/libdwarf/dwarf_print_lines.c
    libdwarf/src/lib/libdwarf/dwarf_query.c
    libdwarf/src/lib/libdwarf/dwarf_ranges.c
    libdwarf/src/lib/libdwarf/dwarf_rnglists.c
    libdwarf/src/lib/libdwarf/dwarf_safe_arithmetic.c
    libdwarf/src/lib/libdwarf/dwarf_safe_strcpy.c
    libdwarf/src/lib/libdwarf/dwarf_secname_ck.c
    libdwarf/src/lib/libdwarf/dwarf_seekr.c
    libdwarf/src/lib/libdwarf/dwarf_setup_sections.c
    libdwarf/src/lib/libdwarf/dwarf_str_offsets.c
    libdwarf/src/lib/libdwarf/dwarf_string.c
    libdwarf/src/lib/libdwarf/dwarf_string.h
    libdwarf/src/lib/libdwarf/dwarf_stringsection.c
    libdwarf/src/lib/libdwarf/dwarf_tied.c
    libdwarf/src/lib/libdwarf/dwarf_tsearchhash.c
    libdwarf/src/lib/libdwarf/dwarf_util.c
    libdwarf/src/lib/libdwarf/dwarf_xu_index.c
)

target_include_directories (dwarf PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/support/libdwarf
    libdwarf/src/lib/libdwarf
)

target_compile_definitions (dwarf PUBLIC
    LIBDWARF_BUILD
)

target_compile_options (dwarf PRIVATE
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
)

target_link_libraries (dwarf PRIVATE z)

target_link_libraries (dwarf PRIVATE libzstd_static)
target_include_directories (dwarf PRIVATE ${zstd_SOURCE_DIR}/lib)

install (
    FILES libdwarf/src/lib/libdwarf/LIBDWARFCOPYRIGHT
    DESTINATION doc
    RENAME LICENSE-libdwarf.txt
)
