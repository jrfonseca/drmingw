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

#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <psapi.h>
#include <dbghelp.h>

#include "outdbg.h"
#include "symbols.h"


BOOL bSymInitialized = FALSE;

BOOL GetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize)
{
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)malloc(sizeof(SYMBOL_INFO) + nSize * sizeof(TCHAR));

    DWORD64 dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol
    BOOL bRet;

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = nSize;

    assert(bSymInitialized);

    bRet = SymFromAddr(hProcess, dwAddress, &dwDisplacement, pSymbol);

    if (bRet) {
        if (UnDecorateSymbolName(pSymbol->Name, lpSymName, nSize, UNDNAME_COMPLETE) == 0) {
            lstrcpyn(lpSymName, pSymbol->Name, nSize);
        }
    }

    free(pSymbol);

    return bRet;
}

BOOL GetLineFromAddr(HANDLE hProcess, DWORD64 dwAddress,  LPTSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
    IMAGEHLP_LINE64 Line;
    DWORD dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

    // Do the source and line lookup.
    memset(&Line, 0, sizeof Line);
    Line.SizeOfStruct = sizeof Line;

    assert(bSymInitialized);

    if(!SymGetLineFromAddr64(hProcess, dwAddress, &dwDisplacement, &Line))
        return FALSE;

    assert(lpFileName && lpLineNumber);

    lstrcpyn(lpFileName, Line.FileName, nSize);
    *lpLineNumber = Line.LineNumber;

    return TRUE;
}

