add_executable (drmingw WIN32
    dialog.cpp
    errmsg.cpp
    main.cpp
    resource.rc
)

target_compile_definitions (drmingw PRIVATE
    PACKAGE="DrMingw"
    VERSION="${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}"
)

target_include_directories (drmingw PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/getoptW
)

set_property (TARGET drmingw APPEND_STRING PROPERTY LINK_FLAGS " -municode")

add_dependencies (drmingw mgwhelp_implib)

target_link_libraries (drmingw
    common
    getoptW
    mgwhelp_implib
)

install (TARGETS drmingw RUNTIME DESTINATION bin)
