/*
 * Copyright 2013-2014 Jose Fonseca
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


#include "tap.h"

#include <assert.h>
#include <stdlib.h>

#include <windows.h>
#include <dbghelp.h>


static bool
comparePath(const char *s1, const char *s2)
{
    while (true) {
        char c1 = *s1++;
        char c2 = *s2++;
        if (c1 == '/') {
            c1 = '\\';
        }
        if (c2 == '/') {
            c2 = '\\';
        }
        if (c2 != c1) {
            return false;
        }
        if (c1 == 0) {
            return true;
        }
    }
}


static BOOL
g_bStripped = FALSE;


static void
checkSym(HANDLE hProcess,
         PVOID pvSymbol,
         const char *szSymbolName)
{
    bool ok;

    DWORD64 dwAddr = (DWORD64)(UINT_PTR)pvSymbol;

    // Test SymFromAddr
    DWORD64 Displacement = 0;
    struct {
        SYMBOL_INFO Symbol;
        CHAR Name[256];
    } s;
    memset(&s, 0, sizeof s);
    s.Symbol.SizeOfStruct = sizeof s.Symbol;
    s.Symbol.MaxNameLen = sizeof s.Symbol.Name + sizeof s.Name;
    ok = SymFromAddr(hProcess, dwAddr, &Displacement, &s.Symbol);
    test_line(ok, "SymFromAddr(&%s)", szSymbolName);
    if (!ok) {
        test_diagnostic_last_error();
    } else {
        if (!g_bStripped) {
            ok = strcmp(s.Symbol.Name, szSymbolName) == 0;
        } else {
            // XXX: ignore differences due to demangling
            ok = strncmp(szSymbolName, s.Symbol.Name, strlen(szSymbolName)) == 0;
        }
        test_line(ok, "SymFromAddr(&%s).Name", szSymbolName);
        if (!ok) {
            test_diagnostic("Name = \"%s\" != \"%s\"",
                            s.Symbol.Name, szSymbolName);
        }
    }
}


static void
checkSymLine(HANDLE hProcess,
             PVOID pvSymbol,
             const char *szSymbolName,
             const char *szFileName,
             DWORD dwLineNumber)
{
    bool ok;

    DWORD64 dwAddr = (DWORD64)(UINT_PTR)pvSymbol;

    checkSym(hProcess, pvSymbol, szSymbolName);

    if (g_bStripped) {
        // Don't check line nos
        return;
    }

    // Test SymGetLineFromAddr64
    DWORD dwDisplacement;
    IMAGEHLP_LINE64 Line;
    ZeroMemory(&Line, sizeof Line);
    Line.SizeOfStruct = sizeof Line;
    ok = SymGetLineFromAddr64(hProcess, dwAddr, &dwDisplacement, &Line);
    test_line(ok, "SymGetLineFromAddr64(&%s)", szSymbolName);
    if (!ok) {
        test_diagnostic_last_error();
    } else {
        ok = comparePath(Line.FileName, szFileName);
        test_line(ok, "SymGetLineFromAddr64(&%s).FileName", szSymbolName);
        if (!ok) {
            test_diagnostic("FileName = \"%s\" != \"%s\"",
                            Line.FileName, szFileName);
        }
        ok = Line.LineNumber == dwLineNumber;
        test_line(ok, "SymGetLineFromAddr64(&%s).LineNumber", szSymbolName);
        if (Line.LineNumber != dwLineNumber) {
            test_diagnostic("LineNumber = %lu != %lu",
                            Line.LineNumber, dwLineNumber);
        }
    }
}


static void
    __attribute__ ((noinline))
checkCaller(HANDLE hProcess,
            const char *szSymbolName,
            const char *szFileName,
            DWORD dwLineNumber)
{
    void *addr = __builtin_return_address(0);
    checkSymLine(hProcess, addr, szSymbolName, szFileName, dwLineNumber);
}


static void
checkExport(HANDLE hProcess,
            const char *szModuleName,
            const char *szSymbolName)
{
    HMODULE hModule = GetModuleHandleA(szModuleName);
    const PVOID pvSymbol = (PVOID)GetProcAddress(hModule, szSymbolName);
    checkSym(hProcess, pvSymbol, szSymbolName);
}



static const DWORD foo_line = __LINE__; static int foo(int a, int b) {
    return a * b;
}


#define LINE_BARRIER rand();


int
main(int argc, char **argv)
{
    HANDLE hProcess = GetCurrentProcess();
    bool ok;

    if (strstr(argv[0], "_stripped_")) {
        g_bStripped = TRUE;
    }

    HMODULE hMgwHelpDll = GetModuleHandleA("mgwhelp.dll");
    if (!hMgwHelpDll) {
        test_line(false, "GetModuleHandleA(\"mgwhelp.dll\")");
    } else {
        test_line(GetProcAddress(hMgwHelpDll, "SymGetOptions") != NULL, "GetProcAddress(\"SymGetOptions\")");
        test_line(GetProcAddress(hMgwHelpDll, "SymGetOptions@0") == NULL, "!GetProcAddress(\"SymGetOptions\")");
    }

    ok = SymInitialize(hProcess, "", TRUE);
    test_line(ok, "SymInitialize()");
    if (!ok) {
        test_diagnostic_last_error();
    } {
        checkSymLine(hProcess, (PVOID)&foo, "foo", __FILE__, foo_line);

        checkCaller(hProcess, "main", __FILE__, __LINE__); LINE_BARRIER

        // Test DbgHelp fallback
        checkExport(hProcess, "kernel32", "Sleep");

        ok = SymCleanup(hProcess);
        test_line(ok, "SymCleanup()");
        if (!ok) {
            test_diagnostic_last_error();
        }
    }

    test_exit();
}
