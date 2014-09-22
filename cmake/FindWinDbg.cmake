# - This module looks for Microsoft Debugging Tools for Windows SDK
# It defines:
#   WINDBG_DIR               : full path to Windbg root dir
#

include (FindPackageMessage)

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
endif (WIN32)

if (WINDBG_DIR)
    set (WINDBG_FOUND 1)
    find_package_message (WINDBG "Found WinDbg: ${WINDBG_DIR}" "[${WINDBG_DIR}]")
endif ()
mark_as_advanced (WINDBG_FOUND)
