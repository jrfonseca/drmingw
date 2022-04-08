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


#include "exchndl.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include <windows.h>
#include <shlwapi.h>

#include "macros.h"
#include "tap.h"


static LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = NULL;
static jmp_buf g_JmpBuf;


static LONG WINAPI
topLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    g_prevExceptionFilter(pExceptionInfo);

    (void)pExceptionInfo;
    longjmp(g_JmpBuf, 1);
}


static char g_szExceptionFunctionPattern[512] = {0};
static char g_szExceptionLinePattern[512] = {0};

static const char *
g_szPatterns[] = {
    " caused an Access Violation ",
#ifdef _WIN64
    " Writing to location 0000000000000000",
#else
    " Writing to location 00000000",
#endif
    g_szExceptionFunctionPattern,
    g_szExceptionLinePattern
};


static void
normalizePath(char *s)
{
    char c;
    while ((c = *s) != '\0') {
        if (c == '/') {
            *s = '\\';
        }
        ++s;
    }
}


int
main(int argc, char **argv)
{
    bool ok;

    const char *szReport = PROG_NAME ".RPT";

    DeleteFileA(szReport);

    g_prevExceptionFilter = SetUnhandledExceptionFilter(topLevelExceptionHandler);

#if !DYNAMIC

    ExcHndlInit();

    ok = ExcHndlSetLogFileNameA(szReport);
    test_line(ok, "ExcHndlSetLogFileNameA(\"%s\")", szReport);

#else

    HMODULE hModule = LoadLibraryA("exchndl.dll");
    ok = hModule != NULL;
    test_line(ok, "LoadLibraryA(\"exchndl.dll\")");
    if (!ok) {
        test_diagnostic_last_error();
        test_exit();
    }

    test_line(!GetProcAddress(hModule, "ExcHndlSetLogFileNameA@4"), "!GetProcAddress(\"ExcHndlSetLogFileNameA@4\")");

    typedef BOOL (APIENTRY * PFN_SETLOGFILENAMEA)(const char *szLogFileName);
    PFN_SETLOGFILENAMEA pfnSetLogFileNameA = (PFN_SETLOGFILENAMEA)GetProcAddress(hModule, "ExcHndlSetLogFileNameA");
    ok = pfnSetLogFileNameA != NULL;
    test_line(ok, "GetProcAddress(\"ExcHndlSetLogFileNameA\")");
    if (!ok) {
        test_diagnostic_last_error();
        test_exit();
    }

    ok = pfnSetLogFileNameA(szReport);
    test_line(ok, "ExcHndlSetLogFileNameA(\"%s\")", szReport);

#endif

    _snprintf(g_szExceptionFunctionPattern, sizeof g_szExceptionFunctionPattern, " %s!%s+0x", PROG_NAME ".exe", __FUNCTION__);

    if (!setjmp(g_JmpBuf) ) {
        _snprintf(g_szExceptionLinePattern, sizeof g_szExceptionLinePattern, "%s @ %u]",
                  PathFindFileNameA(__FILE__), __LINE__); *((volatile int *)0) = 0; LINE_BARRIER
        test_line(false, "longjmp"); exit(1);
    } else {
        test_line(true, "longjmp");
    }

    normalizePath(g_szExceptionFunctionPattern);
    normalizePath(g_szExceptionLinePattern);

    FILE *fp = fopen(szReport, "rt");
    ok = fp != NULL;
    test_line(ok, "fopen(\"%s\")", szReport);
    if (ok) {
        const unsigned nPatterns = _countof(g_szPatterns);
        bool found[nPatterns];
        ZeroMemory(found, sizeof found);

        char szLine[512];

        while (fgets(szLine, sizeof szLine, fp)) {
            normalizePath(szLine);

            for (unsigned i = 0; i < nPatterns; ++i) {
                if (strstr(szLine, g_szPatterns[i])) {
                    found[i] = true;
                }
            }
        }

        for (unsigned i = 0; i < nPatterns; ++i) {
            test_line(found[i], "strstr(\"%s\")", g_szPatterns[i]);
            ok = ok && found[i];
        }

        if (!ok) {
            fseek(fp, 0, SEEK_SET);
            while (fgets(szLine, sizeof szLine, fp)) {
                fprintf(stderr, "%s", szLine);
            }
        }

        fclose(fp);
    }

    test_exit();
}
