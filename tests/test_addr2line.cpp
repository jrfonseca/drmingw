/*
 * Copyright 2015 Jose Fonseca
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
#include <stdio.h>

#include <windows.h>


static void
Foo(void)
{
}


int
main(int argc, char **argv)
{
    const char *szSymbolName = "Foo";
    bool ok;

    HMODULE hModule = GetModuleHandleA(NULL);
    assert(hModule != NULL);

    DWORD64 dwSymbolOffset = (DWORD64)(UINT_PTR)&Foo - (DWORD64)(UINT_PTR)hModule;

    wchar_t szCommand[1024];
    _snwprintf(szCommand, _countof(szCommand), L"addr2line.exe -e %hs -f 0x%llx", argv[0], dwSymbolOffset);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    ok = CreatePipe(&hReadPipe, &hWritePipe, &sa, 0) != FALSE;
    test_line(ok, "CreatePipe");
    if (!ok) {
        test_exit();
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hWritePipe;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};
    ok = CreateProcessW(NULL, szCommand, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) != FALSE;
    test_line(ok, "CreateProcessW(\"%ls\")", szCommand);
    CloseHandle(hWritePipe);
    if (ok) {
        char szOutput[4096];
        DWORD dwTotal = 0;
        while (dwTotal < sizeof(szOutput) - 1) {
            DWORD dwRead;
            if (!ReadFile(hReadPipe, szOutput + dwTotal, DWORD(sizeof(szOutput) - 1 - dwTotal), &dwRead, NULL) ||
                dwRead == 0) {
                break;
            }
            dwTotal += dwRead;
        }
        szOutput[dwTotal] = '\0';

        fprintf(stdout, "%s", szOutput);
        bool found = strstr(szOutput, szSymbolName) != NULL;

        test_line(found, "strstr(\"%s\")", szSymbolName);

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hReadPipe);

    test_exit();
}
