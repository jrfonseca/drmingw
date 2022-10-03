include (CheckCXXCompilerFlag)

if (MINGW)
    # Avoid depending on MinGW runtime DLLs
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_compiler_flag (-static-libgcc HAVE_STATIC_LIBGCC_FLAG)
        if (HAVE_STATIC_LIBGCC_FLAG)
            set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
            set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
            set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc")
        endif ()
    endif ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_compiler_flag (-static-libstdc++ HAVE_STATIC_LIBSTDCXX_FLAG)
        if (HAVE_STATIC_LIBSTDCXX_FLAG)
            set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
            set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
            set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
        endif ()

        # Statically link Posix threads when detected.
        execute_process (
            COMMAND "${CMAKE_COMMAND}" -E echo "#include <thread>\n#ifdef _GLIBCXX_HAS_GTHREADS\n#error _GLIBCXX_HAS_GTHREADS\n#endif"
            COMMAND "${CMAKE_CXX_COMPILER}" -x c++ -E -
            RESULT_VARIABLE STATUS_CXX11_THREADS
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if (NOT STATUS_CXX11_THREADS EQUAL 0)
            # https://stackoverflow.com/a/28001271
            set (CMAKE_CXX_STANDARD_LIBRARIES "-Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic ${CMAKE_CXX_STANDARD_LIBRARIES}")
        endif ()
    endif ()
endif ()

# Use static runtime
if (MSVC)
    # https://cmake.org/cmake/help/v3.15/policy/CMP0091.html
    cmake_policy (GET CMP0091 CMP0091_VALUE)
    if (NOT CMP0091_VALUE STREQUAL NEW)
        message (SEND_ERROR "CMP0091 policy not set to NEW")
    endif ()
    # https://cmake.org/cmake/help/v3.15/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()
