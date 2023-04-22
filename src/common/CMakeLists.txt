add_library (common STATIC
    debugger.cpp
    log.cpp
    symbols.cpp
)

target_link_libraries (common PRIVATE
    psapi
    version
)
