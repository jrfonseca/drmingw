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

target_link_libraries (drmingw PRIVATE
    common
    mgwhelp
)

install (TARGETS drmingw RUNTIME DESTINATION bin)
