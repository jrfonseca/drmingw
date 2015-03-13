add_library (exchndl MODULE
    exchndl.c
    exchndl.def
)

add_dependencies (exchndl mgwhelp_implib)

target_link_libraries (exchndl LINK_PRIVATE
    common
    ${MGWHELP_IMPLIB}
)

set_target_properties (exchndl PROPERTIES
    PREFIX ""
)

install (TARGETS exchndl LIBRARY DESTINATION bin)
