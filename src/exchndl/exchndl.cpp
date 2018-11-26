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

#include "symbols.h"
#include "log.h"
#include "outdbg.h"


#define REPORT_FILE 1


// Declare the static variables
static BOOL g_bHandlerSet = FALSE;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = NULL;
static char g_szLogFileName[MAX_PATH] = "";
static HANDLE g_hReportFile;
static BOOL g_bOwnReportFile;

static void
writeReport(const char *szText)
{
    if (REPORT_FILE) {
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
    } else {
        OutputDebugStringA(szText);
    }
}


static
void GenerateExceptionReport(PEXCEPTION_POINTERS pExceptionInfo)
{
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    // Start out with a banner
    lprintf("-------------------\n\n");

    SYSTEMTIME SystemTime;
    GetLocalTime(&SystemTime);
    char szDateStr[128];
    LCID Locale = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);
    GetDateFormatA(Locale, 0, &SystemTime, "dddd',' MMMM d',' yyyy", szDateStr, _countof(szDateStr));
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
#ifdef _WIN64
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
    lprintf("Windows %lu.%lu.%lu\n",
            osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

    lprintf("DrMingw %u.%u.%u\n",
            PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCH);

    lprintf("\n");
}

#include <stdio.h>
#include <fcntl.h>
#include <io.h>


// Entry point where control comes on an unhandled exception
static
LONG WINAPI TopLevelExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    static LONG cBeenHere = 0;

    if (InterlockedIncrement(&cBeenHere) == 1) {
        UINT fuOldErrorMode;

        fuOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        if (REPORT_FILE) {
            if (!g_hReportFile) {
                if (strcmp(g_szLogFileName, "-") == 0) {
                    g_hReportFile = GetStdHandle(STD_ERROR_HANDLE);
                    g_bOwnReportFile = FALSE;
                } else {
                    g_hReportFile = CreateFileA(
                        g_szLogFileName,
                        GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        0,
                        OPEN_ALWAYS,
                        0,
                        0
                    );
                    g_bOwnReportFile = TRUE;
                }
            }

            if (g_hReportFile) {
                SetFilePointer(g_hReportFile, 0, 0, FILE_END);

                GenerateExceptionReport(pExceptionInfo);

                FlushFileBuffers(g_hReportFile);
            }
        } else {
            GenerateExceptionReport(pExceptionInfo);
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

    if (REPORT_FILE) {
        // Figure out what the report file will be named, and store it away
        if(GetModuleFileNameA(NULL, g_szLogFileName, MAX_PATH))
        {
            LPSTR lpszDot;

            // Look for the '.' before the "EXE" extension.  Replace the extension
            // with "RPT"
            if((lpszDot = strrchr(g_szLogFileName, '.')))
            {
                lpszDot++;    // Advance past the '.'
                strcpy(lpszDot, "RPT");    // "RPT" -> "Report"
            }
            else
                strcat(g_szLogFileName, ".RPT");
        }
        else if(GetWindowsDirectoryA(g_szLogFileName, MAX_PATH))
        {
            strcat(g_szLogFileName, "EXCHNDL.RPT");
        }
    }
}


static void
Cleanup(void)
{
    if (g_hReportFile) {
        if (g_bOwnReportFile) {
            CloseHandle(g_hReportFile);
        }
        g_hReportFile = 0;
    }
}


static void
SetupHandler(void)
{
    // Install the unhandled exception filter function
    if (!g_bHandlerSet) {
        assert(g_prevExceptionFilter == NULL);
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
        g_prevExceptionFilter = NULL;
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
    size_t size = _countof(g_szLogFileName);
    if (!szLogFileName ||
        strlen(szLogFileName) > size - 1) {
        OutputDebug("EXCHNDL: specified log name is too long or invalid (%s)\n",
                    szLogFileName);
        return FALSE;
    }
    strncpy(g_szLogFileName, szLogFileName, size - 1);
    g_szLogFileName[size - 1] = '\0';
    return TRUE;
}


EXTERN_C BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved);

BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpvReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            Setup();
            if (lpvReserved == NULL) {
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
