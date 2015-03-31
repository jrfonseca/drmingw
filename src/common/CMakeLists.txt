add_library (common STATIC
    symbols.c
)

target_link_libraries (common LINK_PRIVATE
    psapi
)
