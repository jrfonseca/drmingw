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

#include "exchndl.h"

#include <assert.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <dbghelp.h>

#include <algorithm>
#include <string>
#include <vector>

#include "symbols.h"
#include "log.h"
#include "outdbg.h"


// Declare the static variables
static BOOL g_bHandlerSet = FALSE;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = nullptr;
static std::string g_szLogFileName;
static HANDLE g_hReportFile = nullptr;
static BOOL g_bOwnReportFile = FALSE;

static void
writeReport(const char *szText)
{
    DWORD cbWritten;
    while (*szText != '\0') {
        const char *p = szText;
        while (*p != '\0' && *p != '\n') {
            ++p;
        }
        WriteFile(g_hReportFile, szText, p - szText, &cbWritten, 0);
        if (*p == '\n') {
            WriteFile(g_hReportFile, "\r\n", 2, &cbWritten, 0);
            ++p;
        }
        szText = p;
    }
}


static void
GenerateExceptionReport(PEXCEPTION_POINTERS pExceptionInfo)
{
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    // Start out with a banner
    lprintf("-------------------\n\n");

    SYSTEMTIME SystemTime;
    GetLocalTime(&SystemTime);
    char szDateStr[128];
    LCID Locale = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    GetDateFormatA(Locale, 0, &SystemTime, "dddd',' MMMM d',' yyyy", szDateStr,
                   _countof(szDateStr));
    char szTimeStr[128];
    GetTimeFormatA(Locale, 0, &SystemTime, "HH':'mm':'ss", szTimeStr, _countof(szTimeStr));
    lprintf("Error occurred on %s at %s.\n\n", szDateStr, szTimeStr);

    HANDLE hProcess = GetCurrentProcess();

    SetSymOptions(FALSE);

    if (InitializeSym(hProcess, TRUE)) {
        dumpException(hProcess, pExceptionRecord);

        PCONTEXT pContext = pExceptionInfo->ContextRecord;
        assert(pContext);

        // XXX: In 64-bits WINE we can get context record that don't match the
        // exception record somehow
#if defined(_M_ARM64)
        PVOID ip = (PVOID)pContext->Pc;
#elif defined(_WIN64)
        PVOID ip = (PVOID)pContext->Rip;
#else
        PVOID ip = (PVOID)pContext->Eip;
#endif
        if (pExceptionRecord->ExceptionAddress != ip) {
            lprintf("warning: inconsistent exception context record\n");
        }

        dumpStack(hProcess, GetCurrentThread(), pContext);

        if (!SymCleanup(hProcess)) {
            assert(0);
        }
    }

    dumpModules(hProcess);

    // TODO: Use GetFileVersionInfo on kernel32.dll as recommended on
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724429.aspx
    // for Windows 10 detection?
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof osvi);
    osvi.dwOSVersionInfoSize = sizeof osvi;
    GetVersionEx(&osvi);
    lprintf("Windows %lu.%lu.%lu\n", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

    lprintf("DrMingw %u.%u.%u\n", PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
            PACKAGE_VERSION_PATCH);

    lprintf("\n");
}

#include <stdio.h>
#include <fcntl.h>
#include <io.h>


// Entry point where control comes on an unhandled exception
static LONG WINAPI
TopLevelExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    static LONG cBeenHere = 0;

    if (InterlockedIncrement(&cBeenHere) == 1) {
        UINT fuOldErrorMode;

        fuOldErrorMode =
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        if (!g_hReportFile) {
            if (g_szLogFileName == "-") {
                g_hReportFile = GetStdHandle(STD_ERROR_HANDLE);
                g_bOwnReportFile = FALSE;
            } else {
                g_hReportFile =
                    CreateFileA(g_szLogFileName.c_str(), GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS, 0, 0);
                g_bOwnReportFile = TRUE;
            }
        }

        if (g_hReportFile) {
            SetFilePointer(g_hReportFile, 0, 0, FILE_END);

            GenerateExceptionReport(pExceptionInfo);

            FlushFileBuffers(g_hReportFile);
        }

        SetErrorMode(fuOldErrorMode);
    }
    InterlockedDecrement(&cBeenHere);

    if (g_prevExceptionFilter)
        return g_prevExceptionFilter(pExceptionInfo);
    else
        return EXCEPTION_CONTINUE_SEARCH;
}


static void
Setup(void)
{
    setDumpCallback(writeReport);

    // Figure out what the report file will be named, and store it away
    DWORD nSize = MAX_PATH;
    std::vector<char> path(nSize);
    DWORD dwRet;
    while ((dwRet = GetModuleFileNameA(nullptr, &path[0], nSize)) == nSize) {
        nSize *= 2;
        path.resize(nSize);
    }
    if (dwRet) {
        // Look for the '.' before the "EXE" extension.  Replace the extension
        // with "RPT"
        path.resize(nSize);
        auto found = std::find(path.crbegin(), path.crend(), '.');
        if (found != path.crend()) {
            path.resize(found - path.crbegin());
        }
        path.push_back('.');
        path.push_back('R');
        path.push_back('P');
        path.push_back('T');
        g_szLogFileName.replace(g_szLogFileName.begin(), g_szLogFileName.end(), &path[0],
                                path.size());
    } else {
        g_szLogFileName = "EXCHNDL.RPT";
    }
}


static void
Cleanup(void)
{
    if (g_hReportFile) {
        if (g_bOwnReportFile) {
            CloseHandle(g_hReportFile);
        }
        g_hReportFile = nullptr;
    }
}


static void
SetupHandler(void)
{
    // Install the unhandled exception filter function
    if (!g_bHandlerSet) {
        assert(g_prevExceptionFilter == nullptr);
        g_prevExceptionFilter = SetUnhandledExceptionFilter(TopLevelExceptionFilter);
        assert(g_prevExceptionFilter != TopLevelExceptionFilter);
        g_bHandlerSet = TRUE;
    }
}


static void
CleanupHandler(void)
{
    if (g_bHandlerSet) {
        SetUnhandledExceptionFilter(g_prevExceptionFilter);
        g_prevExceptionFilter = nullptr;
        g_bHandlerSet = FALSE;
    }
}


VOID APIENTRY
ExcHndlInit(void)
{
    SetupHandler();
}


BOOL APIENTRY
ExcHndlSetLogFileNameA(const char *szLogFileName)
{
    if (!szLogFileName) {
        OutputDebug("EXCHNDL: specified log name is invalid (%s)\n", szLogFileName);
        return FALSE;
    }
    g_szLogFileName = szLogFileName;
    return TRUE;
}


EXTERN_C BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved);

BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved)
{
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        Setup();
        if (lpvReserved == nullptr) {
            SetupHandler();
        }
        break;

    case DLL_PROCESS_DETACH:
        CleanupHandler();
        Cleanup();
        break;
    }

    return TRUE;
}
