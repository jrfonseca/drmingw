add_library (common STATIC
    misc.c
    symbols.c
    pehelp.c
)

target_link_libraries (common LINK_PRIVATE
    psapi
)
