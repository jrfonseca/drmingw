cmake_minimum_required (VERSION 2.8)


project (drmingw)


##############################################################################
# Dependencies

find_path (BINUTILS_INCLUDE_DIR bfd.h PATHS ${BINUTILS_ROOT}/include)
find_library (BINUTILS_BFD_LIBRARY bfd PATHS ${BINUTILS_ROOT}/lib)
find_library (BINUTILS_LIBERTY_LIBRARY liberty PATHS ${BINUTILS_ROOT}/lib)

if (BINUTILS_INCLUDE_DIR AND BINUTILS_BFD_LIBRARY AND BINUTILS_LIBERTY_LIBRARY)
    set (BINUTILS_FOUND 1)
    include_directories (${BINUTILS_INCLUDE_DIR})
endif ()


include_directories (${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/dwarf)
add_subdirectory (thirdparty/dwarf)


##############################################################################
# Set global build options

include (CheckCXXCompilerFlag)

if (MSVC)
    # Use static runtime
    # http://www.cmake.org/Wiki/CMake_FAQ#How_can_I_build_my_MSVC_application_with_a_static_runtime.3F
    foreach (flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
    )
        if (${flag_var} MATCHES "/MD")
            string (REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif ()
    endforeach (flag_var)

    add_definitions (
        # http://msdn2.microsoft.com/en-us/library/6dwk3a1z.aspx,
        -DVC_EXTRALEAN
        -D_CRT_SECURE_NO_DEPRECATE
    )
else ()
    # Adjust warnings
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wmissing-prototypes -std=gnu99")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

    # Use GDB extensions if available
    if (CMAKE_COMPILER_IS_GNUC)
        set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb -O0")
        set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -ggdb")
    endif ()
    if (CMAKE_COMPILER_IS_GNUCXX)
        set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -O0")
        set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -ggdb")
    endif ()
endif ()

if (MINGW)
    # Avoid depending on MinGW runtime DLLs
    check_cxx_compiler_flag (-static-libgcc HAVE_STATIC_LIBGCC_FLAG)
    if (HAVE_STATIC_LIBGCC_FLAG)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc")
    endif ()
    check_cxx_compiler_flag (-static-libstdc++ HAVE_STATIC_LIBSTDCXX_FLAG)
    if (HAVE_STATIC_LIBSTDCXX_FLAG)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
    endif ()

    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--kill-at")
    endif ()
endif ()

add_definitions (
    -D_WIN32_WINNT=0x0601 # minimum required OS version
    -DWINVER=0x0601
)


##############################################################################
# Targets

add_library (common STATIC
    misc.c
    symbols.c
    pehelp.c
)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (MGWHELP_DEF mgwhelp64.def)
else ()
    set (MGWHELP_DEF mgwhelp32.def)
endif ()

add_library (mgwhelp SHARED
    ${MGWHELP_DEF}
    demangle.cxx
    dwarf_pe.c
    mgwhelp.c
)
target_link_libraries (mgwhelp LINK_PRIVATE
    common
    dwarf
    dbghelp
    psapi
)
set_target_properties (mgwhelp PROPERTIES
    PREFIX ""
)
if (BINUTILS_FOUND)
    set_target_properties (mgwhelp PROPERTIES
	COMPILE_DEFINITIONS HAVE_BFD=1
    )
    target_link_libraries (mgwhelp LINK_PRIVATE
        ${BINUTILS_BFD_LIBRARY}
        ${BINUTILS_LIBERTY_LIBRARY}
    )
endif ()
install (TARGETS mgwhelp RUNTIME DESTINATION bin)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    # XXX: We need to use a custom import lib to avoid link failures
    string (REPLACE gcc dlltool DLLTOOL ${CMAKE_C_COMPILER})
    set (MGWHELP_IMPLIB ${CMAKE_BINARY_DIR}/libmgwhelp.a)
    add_custom_command (
        OUTPUT ${MGWHELP_IMPLIB}
        COMMAND ${DLLTOOL} --output-lib ${MGWHELP_IMPLIB} --dllname mgwhelp.dll --kill-at --input-def=${CMAKE_SOURCE_DIR}/dbghelp.def
        DEPENDS ${CMAKE_SOURCE_DIR}/dbghelp.def
    )
else ()
    set (MGWHELP_IMPLIB mgwhelp)
endif ()
add_custom_target (mgwhelp_implib DEPENDS ${MGWHELP_IMPLIB})


add_executable (drmingw WIN32
    debugger.c
    dialog.c
    log.c
    main.c
    resource.rc
)
add_dependencies (drmingw mgwhelp_implib)
target_link_libraries (drmingw
    common
    ${MGWHELP_IMPLIB}
    psapi
)
install (TARGETS drmingw RUNTIME DESTINATION bin)

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

add_subdirectory (sample)
add_subdirectory (tests)


##############################################################################
# Packaging

install (
    FILES
        COPYING
        COPYING.LIB
        README.txt
    DESTINATION doc
)

set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "6")
set (CPACK_PACKAGE_VERSION_PATCH "1")

# cpack mistakenly detects Mingw-w64 as win32
if (MINGW)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set (CPACK_SYSTEM_NAME win64)
    endif ()
endif ()

set (CPACK_GENERATOR "ZIP")

include(CPack)
