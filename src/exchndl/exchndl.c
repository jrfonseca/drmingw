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
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <dbghelp.h>

#include "symbols.h"
#include "log.h"
#include "outdbg.h"


#define REPORT_FILE 1


// Declare the static variables
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevExceptionFilter = NULL;
static TCHAR g_szLogFileName[MAX_PATH] = _T("");
static HANDLE g_hReportFile;

static void
writeReport(const char *szText)
{
    if (REPORT_FILE) {
        DWORD cbWritten;
        WriteFile(g_hReportFile, szText, strlen(szText), &cbWritten, 0);
    } else {
        OutputDebugStringA(szText);
    }
}


static
void GenerateExceptionReport(PEXCEPTION_POINTERS pExceptionInfo)
{
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    // Start out with a banner
    lprintf(_T("-------------------\r\n\r\n"));

    SYSTEMTIME SystemTime;
    GetLocalTime(&SystemTime);
    TCHAR szDateStr[128];
    GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, &SystemTime, _T("dddd',' MMMM d',' yyyy"), szDateStr, _countof(szDateStr));
    TCHAR szTimeStr[128];
    GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, &SystemTime, _T("HH':'mm':'ss"), szTimeStr, _countof(szTimeStr));
    lprintf(_T("Error occured on %s at %s.\r\n\r\n"), szDateStr, szTimeStr);

    HANDLE hProcess = GetCurrentProcess();

    DWORD dwSymOptions = SymGetOptions();
    dwSymOptions |=
        SYMOPT_LOAD_LINES |
        SYMOPT_DEFERRED_LOADS;
    SymSetOptions(dwSymOptions);
    if (InitializeSym(hProcess, TRUE)) {

        dumpException(hProcess, pExceptionRecord);

        PCONTEXT pContext = pExceptionInfo->ContextRecord;

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
    lprintf(_T("Windows %lu.%lu.%lu\r\n"),
            osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

    lprintf(_T("DrMingw %u.%u.%u\r\n"),
            PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCH);

    lprintf(_T("\r\n"));
}

#include <stdio.h>
#include <fcntl.h>
#include <io.h>


// Entry point where control comes on an unhandled exception
static
LONG WINAPI TopLevelExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    static BOOL bBeenHere = FALSE;

    if(!bBeenHere)
    {
        UINT fuOldErrorMode;

        bBeenHere = TRUE;

        fuOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        if (REPORT_FILE) {
            g_hReportFile = CreateFile(
                g_szLogFileName,
                GENERIC_WRITE,
                0,
                0,
                OPEN_ALWAYS,
                FILE_FLAG_WRITE_THROUGH,
                0
            );

            if (g_hReportFile) {
                SetFilePointer(g_hReportFile, 0, 0, FILE_END);

                GenerateExceptionReport(pExceptionInfo);

                CloseHandle(g_hReportFile);
                g_hReportFile = 0;
            }
        } else {
            GenerateExceptionReport(pExceptionInfo);
        }

        SetErrorMode(fuOldErrorMode);
    }

    if (g_prevExceptionFilter)
        return g_prevExceptionFilter(pExceptionInfo);
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

static void OnStartup(void)
{
    // Install the unhandled exception filter function
    g_prevExceptionFilter = SetUnhandledExceptionFilter(TopLevelExceptionFilter);

    setDumpCallback(writeReport);

    if (REPORT_FILE) {
        // Figure out what the report file will be named, and store it away
        if(GetModuleFileName(NULL, g_szLogFileName, MAX_PATH))
        {
            LPTSTR lpszDot;

            // Look for the '.' before the "EXE" extension.  Replace the extension
            // with "RPT"
            if((lpszDot = _tcsrchr(g_szLogFileName, _T('.'))))
            {
                lpszDot++;    // Advance past the '.'
                _tcscpy(lpszDot, _T("RPT"));    // "RPT" -> "Report"
            }
            else
                _tcscat(g_szLogFileName, _T(".RPT"));
        }
        else if(GetWindowsDirectory(g_szLogFileName, MAX_PATH))
        {
            _tcscat(g_szLogFileName, _T("EXCHNDL.RPT"));
        }
    }
}

static void OnExit(void)
{
    SetUnhandledExceptionFilter(g_prevExceptionFilter);
}


BOOL APIENTRY
SetLogFileNameA(const char *szLogFileName)
{
    size_t size = _countof(g_szLogFileName);
    if (!szLogFileName ||
        _tcslen(szLogFileName) > size - 1) {
        OutputDebug("EXCHNDL: specified log name is too long or invalid (%s)\n",
                    szLogFileName);
        return FALSE;
    }
    _tcsncpy(g_szLogFileName, szLogFileName, size - 1);
    g_szLogFileName[size - 1] = _T('\0');
    return TRUE;
}


BOOL APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            OnStartup();
            break;

        case DLL_PROCESS_DETACH:
            OnExit();
            break;
    }

    return TRUE;
}
