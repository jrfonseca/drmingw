/*
 * Copyright 2014 Jose Fonseca
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


/*
 * Simple addr2line like utility.
 */


#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include <windows.h>
#include <dbghelp.h>

#include "symbols.h"


static BOOL CALLBACK
callback(HANDLE hProcess,
         ULONG ActionCode,
         ULONG64 CallbackData,
         ULONG64 UserContext)
{
    if (ActionCode == CBA_DEBUG_INFO) {
        fputs((LPCSTR)(UINT_PTR)CallbackData, stderr);
        return TRUE;
    }

    return FALSE;
}


int
main(int argc, char **argv)
{
    BOOL bRet;
    DWORD dwRet;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <dll> <addr> ...\n", argv[0]);
        return 1;
    }

    // Load the module
    char *szModule = argv[1];
    HMODULE hModule = LoadLibraryExA(szModule, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!hModule) {
        LoadLibraryExA(szModule, NULL, DONT_RESOLVE_DLL_REFERENCES);
    }
    if (!hModule) {
        fprintf(stderr, "error: failed to load %s\n", szModule);
        return 1;
    }

    DWORD dwSymOptions = SymGetOptions();

    dwSymOptions |= SYMOPT_LOAD_LINES;

#ifndef NDEBUG
    dwSymOptions |= SYMOPT_DEBUG;
#endif

    // We can get more information by calling UnDecorateSymbolName() ourselves.
    if (0) {
        dwSymOptions |= SYMOPT_UNDNAME;
    } else {
        dwSymOptions &= ~SYMOPT_UNDNAME;
    }
    
    SymSetOptions(dwSymOptions);


    HANDLE hProcess = GetCurrentProcess();
    bRet = InitializeSym(hProcess, FALSE);
    assert(bRet);


    SymRegisterCallback64(hProcess, &callback, 0);


    dwRet = SymLoadModule64(hProcess, NULL, szModule, NULL, (DWORD64)(UINT_PTR)hModule, 0);
    if (!dwRet) {
        fprintf(stderr, "error: failed to load module symbols\n");
    }

    if (!GetModuleHandleA("symsrv.dll")) {
        fprintf(stderr, "symbol server not loaded\n");
    }


    for (int i = 2; i < argc; ++i) {
        const char *arg = argv[i];
        DWORD64 dwRelAddr;
        if (arg[0] == '0' && arg[1] == 'x') {
            sscanf(&arg[2], "%08I64X", &dwRelAddr);
        } else {
            dwRelAddr = atol(arg);
        }
        printf("dwRelAddr = 0x%08I64X\n", dwRelAddr);

        UINT_PTR dwAddr = (UINT_PTR)hModule + dwRelAddr;

        struct {
            SYMBOL_INFO Symbol;
            CHAR Name[512];
        } sym;
        ZeroMemory(&sym, sizeof sym);
        sym.Symbol.SizeOfStruct = sizeof sym.Symbol;
        sym.Symbol.MaxNameLen = sizeof sym.Symbol.Name + sizeof sym.Name;
        DWORD64 dwSymDisplacement = 0;
        bRet = SymFromAddr(hProcess, dwAddr, &dwSymDisplacement, &sym.Symbol);
        if (bRet) {
            printf("Symbol.Name = %s\n", sym.Symbol.Name);
            char UnDecoratedName[512];
            if (UnDecorateSymbolName( sym.Symbol.Name, UnDecoratedName, sizeof UnDecoratedName, UNDNAME_COMPLETE)) {
                printf("UnDecoratedName = %s\n", UnDecoratedName);
            }
        }

        IMAGEHLP_LINE64 line;
        ZeroMemory(&line, sizeof line);
        line.SizeOfStruct = sizeof line;
        DWORD dwLineDisplacement = 0;
        bRet = SymGetLineFromAddr64(hProcess, dwAddr, &dwLineDisplacement, &line);
        if (bRet) {
            printf("FileName = %s\n", line.FileName);
            printf("LineNumber = %lu\n", line.LineNumber);
        }

        printf("\n");
    }


    SymCleanup(hProcess);


    FreeLibrary(hModule);


    return 0;
}
