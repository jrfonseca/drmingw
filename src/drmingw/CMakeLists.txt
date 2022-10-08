add_executable (drmingw WIN32
    dialog.cpp
    errmsg.cpp
    main.cpp
    resource.rc
)

set_target_properties (drmingw PROPERTIES
    COMPILE_DEFINITIONS "PACKAGE=\"DrMingw\";VERSION=\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\""
)

add_dependencies (drmingw mgwhelp_implib)

target_link_libraries (drmingw
    common
    mgwhelp_implib
)

install (TARGETS drmingw RUNTIME DESTINATION bin)
