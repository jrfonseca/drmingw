cmake_minimum_required (VERSION 2.8)


project (drmingw)


##############################################################################
# Find dependencies

find_path (BINUTILS_INCLUDE_DIR bfd.h PATHS ${BINUTILS_ROOT}/include)
find_library (BINUTILS_BFD_LIBRARY bfd PATHS ${BINUTILS_ROOT}/lib)
find_library (BINUTILS_LIBERTY_LIBRARY liberty PATHS ${BINUTILS_ROOT}/lib)

include_directories (${BINUTILS_INCLUDE_DIR})


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
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wmissing-prototypes")
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
endif ()

add_definitions (
	-D_WIN32_WINNT=0x0601 # minimum required OS version
	-DWINVER=0x0601
)


##############################################################################
# Automatic header generation

find_program (CFUNCTIONS_EXECUTABLE cfunctions)
if (CFUNCTIONS_EXECUTABLE)
	set (HEADERS)

	foreach (MODULE debugger dialog log misc symbols)
		add_custom_command (
			OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.h
			COMMAND ${CFUNCTIONS_EXECUTABLE} -i ${MODULE}.c
			DEPENDS ${MODULE}.c
		)
	set (HEADERS ${HEADERS} ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}.h)
	endforeach (MODULE)

	add_custom_target (headers DEPENDS ${HEADERS})
endif ()


##############################################################################
# Targets

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library (common STATIC
	misc.c
	symbols.c
)
target_link_libraries (common
	psapi
	${BINUTILS_BFD_LIBRARY}
	${BINUTILS_LIBERTY_LIBRARY}
)

add_executable (drmingw WIN32
	debugger.c
	dialog.c
	log.c
	main.c
	resource.rc
)

target_link_libraries (drmingw
	common
)

install (TARGETS drmingw RUNTIME DESTINATION bin)

add_library (exchndl MODULE
	exchndl.c
	exchndl.def
)

target_link_libraries (exchndl
	common
)

set_target_properties (exchndl PROPERTIES
    PREFIX ""
)

install (TARGETS exchndl LIBRARY DESTINATION lib)

add_subdirectory (samples)


##############################################################################
# Packaging

install (
	FILES
		COPYING
		COPYING.LIB
		doc/drmingw.html
		doc/drmingw.reg
		doc/exception-nt.gif
		doc/install.gif
		doc/sample.gif
	DESTINATION doc
)

set (CPACK_PACKAGE_VERSION_MAJOR "0")
set (CPACK_PACKAGE_VERSION_MINOR "4")
set (CPACK_PACKAGE_VERSION_PATCH "4")

# cpack mistakenly detects Mingw-w64 as win32
if (MINGW)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set (CPACK_SYSTEM_NAME win64)
	endif ()
endif ()

set (CPACK_GENERATOR "ZIP")

include(CPack)
