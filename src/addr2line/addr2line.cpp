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

#include <windows.h>
#include <dbghelp.h>

#include <getopt.h>

#include "symbols.h"
#include "wine.h"


static void
usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s -e EXECUTABLE ADDRESS ...\n"
            "\n"
            "options:\n"
            "  -C             demangle C++ function names\n"
            "  -D             enables debugging output (for debugging addr2line itself)\n"
            "  -e EXECUTABLE  specify the EXE/DLL\n"
            "  -f             show functions\n"
            "  -H             displays command line help text\n"
            "  -p             pretty print\n",
            argv0);
}


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
    bool debug = false;
    char *szModule = nullptr;
    bool functions = false;
    bool demangle = false;
    bool pretty = false;

    while (1) {
        int opt = getopt(argc, argv, "?CDe:fHp");

        switch (opt) {
        case 'C':
            demangle = true;
            break;
        case 'D':
            debug = true;
            break;
        case 'e':
            szModule = optarg;
            break;
        case 'f':
            functions = true;
            break;
        case 'H':
            usage(argv[0]);
            return EXIT_SUCCESS;
        case 'p':
            pretty = true;
            break;
        case '?':
            fprintf(stderr, "error: invalid option `%c`\n", optopt);
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
    hModule = LoadLibraryExA(szModule, NULL, LOAD_LIBRARY_AS_DATAFILE);
#endif
    if (!hModule) {
        hModule = LoadLibraryExA(szModule, NULL, DONT_RESOLVE_DLL_REFERENCES);
    }
    if (!hModule) {
        fprintf(stderr, "error: failed to load %s\n", szModule);
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
    assert(bRet);

    if (debug) {
        SymRegisterCallback64(hProcess, &callback, 0);
    }

    dwRet = SymLoadModuleEx(hProcess, NULL, szModule, NULL, BaseOfDll, 0, NULL, 0);
    if (!dwRet) {
        fprintf(stderr, "warning: failed to load module symbols\n");
    }

    if (!GetModuleHandleA("symsrv.dll")) {
        fprintf(stderr, "warning: symbol server not loaded\n");
    }

    while (optind < argc) {
        const char *arg = argv[optind++];

        DWORD64 dwRelAddr;
        if (arg[0] == '0' && arg[1] == 'x') {
            sscanf(&arg[2], "%08" PRIX64, &dwRelAddr);
        } else {
            dwRelAddr = atol(arg);
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
                    if (UnDecorateSymbolName( sym.Symbol.Name, UnDecoratedName, sizeof UnDecoratedName, UNDNAME_COMPLETE)) {
                        function = UnDecoratedName;
                    }
                }
            }
            fputs(function, stdout);
            fputs(pretty ? " at " : "\n", stdout);
        }

        IMAGEHLP_LINE64 line;
        ZeroMemory(&line, sizeof line);
        line.SizeOfStruct = sizeof line;
        DWORD dwLineDisplacement = 0;
        bRet = SymGetLineFromAddr64(hProcess, dwAddr, &dwLineDisplacement, &line);
        if (bRet) {
            fprintf(stdout, "%s:%lu\n", line.FileName, line.LineNumber);
        } else {
            fputs("??:?\n", stdout);
        }
        fflush(stdout);
    }


    SymCleanup(hProcess);


    FreeLibrary(hModule);


    return 0;
}
