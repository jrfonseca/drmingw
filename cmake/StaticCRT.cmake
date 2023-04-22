include (CheckCXXCompilerFlag)

if (MINGW)
    # Avoid depending on MinGW runtime DLLs
    add_link_options (-static)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_link_options (-static-libgcc -static-libstdc++)
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
