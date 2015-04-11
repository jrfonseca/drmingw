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
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <dbghelp.h>

#include "symbols.h"
#include "log.h"


#define REPORT_FILE 1


// Declare the static variables
static LPTOP_LEVEL_EXCEPTION_FILTER prevExceptionFilter = NULL;
static TCHAR szLogFileName[MAX_PATH] = _T("");
static HANDLE hReportFile;

static void
writeReport(const char *szText)
{
    if (REPORT_FILE) {
        DWORD cbWritten;
        WriteFile(hReportFile, szText, strlen(szText), &cbWritten, 0);
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


    assert(!bSymInitialized);

    DWORD dwSymOptions = SymGetOptions();
    dwSymOptions |=
        SYMOPT_LOAD_LINES |
        SYMOPT_DEFERRED_LOADS;
    SymSetOptions(dwSymOptions);
    if (SymInitialize(hProcess, "srv*C:\\Symbols*http://msdl.microsoft.com/download/symbols", TRUE)) {
        bSymInitialized = TRUE;

        dumpException(hProcess, pExceptionRecord);

        PCONTEXT pContext = pExceptionInfo->ContextRecord;

        dumpStack(hProcess, GetCurrentThread(), pContext);

        if (!SymCleanup(hProcess))
            assert(0);

        bSymInitialized = FALSE;
    }
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
            hReportFile = CreateFile(
                szLogFileName,
                GENERIC_WRITE,
                0,
                0,
                OPEN_ALWAYS,
                FILE_FLAG_WRITE_THROUGH,
                0
            );

            if (hReportFile) {
                SetFilePointer(hReportFile, 0, 0, FILE_END);

                GenerateExceptionReport(pExceptionInfo);

                CloseHandle(hReportFile);
                hReportFile = 0;
            }
        } else {
            GenerateExceptionReport(pExceptionInfo);
        }

        SetErrorMode(fuOldErrorMode);
    }

    if (prevExceptionFilter)
        return prevExceptionFilter(pExceptionInfo);
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

static void OnStartup(void)
{
    // Install the unhandled exception filter function
    prevExceptionFilter = SetUnhandledExceptionFilter(TopLevelExceptionFilter);

    setDumpCallback(writeReport);

    if (REPORT_FILE) {
        // Figure out what the report file will be named, and store it away
        if(GetModuleFileName(NULL, szLogFileName, MAX_PATH))
        {
            LPTSTR lpszDot;

            // Look for the '.' before the "EXE" extension.  Replace the extension
            // with "RPT"
            if((lpszDot = _tcsrchr(szLogFileName, _T('.'))))
            {
                lpszDot++;    // Advance past the '.'
                _tcscpy(lpszDot, _T("RPT"));    // "RPT" -> "Report"
            }
            else
                _tcscat(szLogFileName, _T(".RPT"));
        }
        else if(GetWindowsDirectory(szLogFileName, MAX_PATH))
        {
            _tcscat(szLogFileName, _T("EXCHNDL.RPT"));
        }
    }
}

static void OnExit(void)
{
    SetUnhandledExceptionFilter(prevExceptionFilter);
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
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
