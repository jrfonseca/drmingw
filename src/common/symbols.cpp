/*
 * Copyright 2002-2013 Jose Fonseca
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <assert.h>
#include <stdlib.h>

#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#include <dbghelp.h>

#include "outdbg.h"
#include "symbols.h"


EXTERN_C DWORD
SetSymOptions(BOOL fDebug)
{
    DWORD dwSymOptions = SymGetOptions();

    // We have more control calling UnDecorateSymbolName directly Also, we
    // don't want DbgHelp trying to undemangle MinGW symbols (e.g., from DLL
    // exports) behind our back (as it will just strip the leading underscore.)
    if (0) {
        dwSymOptions |= SYMOPT_UNDNAME;
    } else {
        dwSymOptions &= ~SYMOPT_UNDNAME;
    }

    dwSymOptions |=
        SYMOPT_LOAD_LINES |
        SYMOPT_OMAP_FIND_NEAREST;

    if (TRUE) {
        dwSymOptions |= SYMOPT_DEFERRED_LOADS;
    }

    if (fDebug) {
        dwSymOptions |= SYMOPT_DEBUG;
    }

#ifdef _WIN64
    dwSymOptions |= SYMOPT_INCLUDE_32BIT_MODULES;
#endif

    return SymSetOptions(dwSymOptions);
}


EXTERN_C BOOL
InitializeSym(HANDLE hProcess, BOOL fInvadeProcess)
{
    // Provide default symbol search path
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms680689.aspx
    // http://msdn.microsoft.com/en-gb/library/windows/hardware/ff558829.aspx
    char szSymSearchPathBuf[MAX_PATH * 2];
    const char *szSymSearchPath = NULL;
    if (getenv("_NT_SYMBOL_PATH") == NULL &&
        getenv("_NT_ALT_SYMBOL_PATH") == NULL) {
        char szLocalAppData[MAX_PATH];
        HRESULT hr = SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szLocalAppData);
        assert(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) {
            _snprintf(szSymSearchPathBuf,
                      sizeof szSymSearchPathBuf,
                      "srv*%s\\drmingw*http://msdl.microsoft.com/download/symbols",
                      szLocalAppData);
            szSymSearchPath = szSymSearchPathBuf;
        } else {
            // No cache
            szSymSearchPath = "srv*http://msdl.microsoft.com/download/symbols";
        }
    }

    return SymInitialize(hProcess, szSymSearchPath, fInvadeProcess);
}



BOOL GetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPSTR lpSymName, DWORD nSize)
{
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)malloc(sizeof(SYMBOL_INFO) + nSize * sizeof(char));

    DWORD64 dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol
    BOOL bRet;

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = nSize;

    DWORD dwOptions = SymGetOptions();

    bRet = SymFromAddr(hProcess, dwAddress, &dwDisplacement, pSymbol);

    if (bRet) {
        // Demangle if not done already
        if ((dwOptions & SYMOPT_UNDNAME) ||
            UnDecorateSymbolName(pSymbol->Name, lpSymName, nSize, UNDNAME_NAME_ONLY) == 0) {
            strncpy(lpSymName, pSymbol->Name, nSize);
        }
    }

    free(pSymbol);

    return bRet;
}

BOOL GetLineFromAddr(HANDLE hProcess, DWORD64 dwAddress,  LPSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
    IMAGEHLP_LINE64 Line;
    DWORD dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

    // Do the source and line lookup.
    memset(&Line, 0, sizeof Line);
    Line.SizeOfStruct = sizeof Line;

    if(!SymGetLineFromAddr64(hProcess, dwAddress, &dwDisplacement, &Line))
        return FALSE;

    assert(lpFileName && lpLineNumber);

    strncpy(lpFileName, Line.FileName, nSize);
    *lpLineNumber = Line.LineNumber;

    return TRUE;
}

