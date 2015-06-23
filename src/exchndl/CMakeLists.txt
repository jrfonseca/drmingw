if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (EXCHNDL_EXP_DEF exchndl32exp.def)
    set (EXCHNDL_IMP_DEF exchndl32imp.def)
else ()
    set (EXCHNDL_EXP_DEF exchndl64.def)
    set (EXCHNDL_IMP_DEF exchndl64.def)
endif ()

add_library (exchndl MODULE
    exchndl.c
    ${EXCHNDL_EXP_DEF}
    version.rc
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


set (EXCHNDL_IMPLIB ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libexchndl.a)
add_custom_command (
    OUTPUT ${EXCHNDL_IMPLIB}
    COMMAND ${DLLTOOL} --output-lib ${EXCHNDL_IMPLIB} --kill-at --input-def=${CMAKE_CURRENT_SOURCE_DIR}/${EXCHNDL_IMP_DEF}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${EXCHNDL_IMP_DEF}
)
add_custom_target (exchndl_implib ALL DEPENDS exchndl ${EXCHNDL_IMPLIB})
install (FILES ${EXCHNDL_IMPLIB} DESTINATION lib)
