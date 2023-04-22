if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_C_COMPILER_ID STREQUAL GNU)
    add_library (exchndl SHARED exchndl32.def)
else ()
    add_library (exchndl SHARED exchndl64.def)
endif ()

target_sources (exchndl PRIVATE
    exchndl.cpp
    version.rc
)

target_link_libraries (exchndl PRIVATE
    common
    mgwhelp
)

set_target_properties (exchndl PROPERTIES
    PREFIX ""
)

install (TARGETS exchndl LIBRARY DESTINATION bin)
