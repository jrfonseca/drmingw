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


#define __STDC_FORMAT_MACROS 1

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <wchar.h>

#include <io.h>
#include <fcntl.h>

#include <windows.h>
#include <dbghelp.h>

#include <getoptW.h>

#include "symbols.h"
#include "wine.h"


EXTERN_C BOOL IMAGEAPI SymRegisterCallbackW64(HANDLE, PSYMBOL_REGISTERED_CALLBACK64, ULONG64);


static void
usage(const wchar_t *argv0)
{
    fwprintf(stderr,
             L"usage: %ls -e EXECUTABLE ADDRESS ...\n"
             L"\n"
             L"options:\n"
             L"  -C             demangle C++ function names\n"
             L"  -D             enables debugging output (for debugging addr2line itself)\n"
             L"  -e EXECUTABLE  specify the EXE/DLL\n"
             L"  -f             show functions\n"
             L"  -H             displays command line help text\n"
             L"  -p             pretty print\n",
             argv0);
}


static BOOL CALLBACK
callback(HANDLE hProcess, ULONG ActionCode, ULONG64 CallbackData, ULONG64 UserContext)
{
    if (ActionCode == CBA_DEBUG_INFO) {
        fputws((LPCWSTR)(UINT_PTR)CallbackData, stderr);
        return TRUE;
    }

    return FALSE;
}


int
wmain(int argc, wchar_t **argv)
{
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    BOOL bRet;
    DWORD dwRet;
    bool debug = false;
    wchar_t *szModule = nullptr;
    bool functions = false;
    bool demangle = false;
    bool pretty = false;

    while (1) {
        int opt = getoptW(argc, argv, L"?CDe:fHp");

        switch (opt) {
        case L'C':
            demangle = true;
            break;
        case L'D':
            debug = true;
            break;
        case L'e':
            szModule = optarg;
            break;
        case L'f':
            functions = true;
            break;
        case L'H':
            usage(argv[0]);
            return EXIT_SUCCESS;
        case L'p':
            pretty = true;
            break;
        case L'?':
            fwprintf(stderr, L"error: invalid option `%lc`\n", optopt);
            /* pass-through */
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        case -1:
            break;
        }
        if (opt == -1) {
            break;
        }
    }

    if (szModule == nullptr) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Load the module
    HMODULE hModule = nullptr;
#ifdef _WIN64
    // XXX: The GetModuleFileName function does not retrieve the path for
    // modules that were loaded using the LOAD_LIBRARY_AS_DATAFILE flag
    hModule = LoadLibraryExW(szModule, NULL, LOAD_LIBRARY_AS_DATAFILE);
#endif
    if (!hModule) {
        hModule = LoadLibraryExW(szModule, NULL, DONT_RESOLVE_DLL_REFERENCES);
    }
    if (!hModule) {
        fwprintf(stderr, L"error: failed to load %ls\n", szModule);
        return EXIT_FAILURE;
    }

    // Handles for modules loaded with DATAFILE/IMAGE_RESOURCE flags have lower
    // bits set
    DWORD64 BaseOfDll = (DWORD64)(UINT_PTR)hModule;
    BaseOfDll &= ~DWORD64(3);

    DWORD dwSymOptions = SymGetOptions();

    dwSymOptions |= SYMOPT_LOAD_LINES;

#ifndef NDEBUG
    dwSymOptions |= SYMOPT_DEBUG;
#endif

    // We can get more information by calling UnDecorateSymbolName() ourselves.
    dwSymOptions &= ~SYMOPT_UNDNAME;

    SymSetOptions(dwSymOptions);

    HANDLE hProcess = GetCurrentProcess();
    bRet = InitializeSym(hProcess, FALSE);
    if (!bRet) {
        fwprintf(stderr, L"warning: failed to initialize DbgHelp\n");
        return EXIT_FAILURE;
    }

    if (debug) {
        SymRegisterCallbackW64(hProcess, &callback, 0);
    }

    dwRet = SymLoadModuleExW(hProcess, NULL, szModule, NULL, BaseOfDll, 0, NULL, 0);
    if (!dwRet) {
        fwprintf(stderr, L"warning: failed to load module symbols\n");
    }

    if (!GetModuleHandleA("symsrv.dll")) {
        fwprintf(stderr, L"warning: symbol server not loaded\n");
    }

    while (optind < argc) {
        const wchar_t *arg = argv[optind++];

        DWORD64 dwRelAddr;
        if (arg[0] == L'0' && arg[1] == L'x') {
            dwRelAddr = wcstoull(&arg[2], nullptr, 16);
        } else {
            dwRelAddr = wcstoull(arg, nullptr, 10);
        }

        UINT_PTR dwAddr = BaseOfDll + dwRelAddr;

        if (functions) {
            struct {
                SYMBOL_INFO Symbol;
                CHAR Name[512];
            } sym;
            char UnDecoratedName[512];
            const char *function = "??";
            ZeroMemory(&sym, sizeof sym);
            sym.Symbol.SizeOfStruct = sizeof sym.Symbol;
            sym.Symbol.MaxNameLen = sizeof sym.Symbol.Name + sizeof sym.Name;
            DWORD64 dwSymDisplacement = 0;
            bRet = SymFromAddr(hProcess, dwAddr, &dwSymDisplacement, &sym.Symbol);
            if (bRet) {
                function = sym.Symbol.Name;
                if (demangle) {
                    if (UnDecorateSymbolName(sym.Symbol.Name, UnDecoratedName,
                                             sizeof UnDecoratedName, UNDNAME_COMPLETE)) {
                        function = UnDecoratedName;
                    }
                }
            }
            fwprintf(stdout, L"%hs", function);
            fputws(pretty ? L" at " : L"\n", stdout);
        }

        IMAGEHLP_LINEW64 line;
        ZeroMemory(&line, sizeof line);
        line.SizeOfStruct = sizeof line;
        DWORD dwLineDisplacement = 0;
        bRet = SymGetLineFromAddrW64(hProcess, dwAddr, &dwLineDisplacement, &line);
        if (bRet) {
            fwprintf(stdout, L"%ls:%lu\n", line.FileName, line.LineNumber);
        } else {
            fputws(L"??:?\n", stdout);
        }
        fflush(stdout);
    }


    SymCleanup(hProcess);


    FreeLibrary(hModule);


    return 0;
}
