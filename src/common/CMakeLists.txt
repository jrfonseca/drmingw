add_library (common STATIC
    debugger.c
    log.c
    symbols.c
)

target_link_libraries (common LINK_PRIVATE
    psapi
)
