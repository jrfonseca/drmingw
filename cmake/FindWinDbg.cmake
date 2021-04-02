# - This module looks for Microsoft Debugging Tools for Windows SDK
# It defines:
#   WINDBG_DIR               : full path to Windbg root dir
#

include (FindPackageHandleStandardArgs)

if (WIN32)
    find_path (WINDBG_DIR
        NAMES
            dbghelp.dll
            symsrv.dll
            symsrv.yes
        PATHS
            "$ENV{ProgramFiles}/Debugging Tools for Windows/sdk"
            "$ENV{ProgramFiles}/Debugging Tools for Windows (x86)/sdk"
            "$ENV{ProgramFiles}/Debugging Tools for Windows (x64)/sdk"
        NO_DEFAULT_PATH
        DOC "Microsoft Debugging Tools"
    )
    mark_as_advanced (WINDBG_DIR)
endif ()

find_package_handle_standard_args (WinDbg DEFAULT_MSG WINDBG_DIR)
