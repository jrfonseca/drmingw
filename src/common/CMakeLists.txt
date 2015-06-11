add_library (common STATIC
    debugger.cpp
    log.c
    symbols.c
)

target_link_libraries (common LINK_PRIVATE
    psapi
    version
)
