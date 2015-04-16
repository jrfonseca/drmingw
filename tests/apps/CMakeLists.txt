cmake_minimum_required (VERSION 2.6)

project (drmingw-test-apps)

unset (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
unset (CMAKE_LIBRARY_OUTPUT_DIRECTORY)

if (MSVC)
    # http://www.cmake.org/Wiki/CMake_FAQ#How_can_I_build_my_MSVC_application_with_a_static_runtime.3F
    foreach (flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
    )
        if (${flag_var} MATCHES "/MD")
            string (REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif ()
    endforeach ()

    add_definitions (-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)
endif ()


add_executable (_exit3 _exit3.c)
add_executable (abort_console abort_console.c)
add_executable (abort_gui WIN32 abort_gui.c)
add_executable (assert_console assert_console.c)
add_executable (assert_gui WIN32 assert_gui.c)
add_executable (debug_break debug_break.c)
add_executable (access_violation access_violation.c)
add_executable (access_violation_cxx access_violation.cpp)
add_executable (false false.c)
add_executable (infinite_loop infinite_loop.c)
add_executable (int3 int3.c)
add_executable (is_debugger_present is_debugger_present.c)
add_executable (message_box WIN32 message_box.c)
add_executable (output_debug_string WIN32 output_debug_string.c)
add_executable (seh WIN32 seh.c)
add_executable (std_exception std_exception.cpp)
add_executable (std_terminate std_terminate.cpp)
add_executable (true true.c)
