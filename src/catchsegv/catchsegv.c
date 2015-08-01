/*
 * Copyright 2009-2015 Jose Fonseca
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
 * Simple catchsegv like utility.
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <windows.h>
#include <dbghelp.h>

#include <getopt.h>

#include "log.h"
#include "debugger.h"
#include "symbols.h"


static void
outputCallback(const char *s)
{
    fputs(s, stderr);
    fflush(stderr);
}



static ULONG g_TimeOut = 0;
static char g_CommandLine[4096];
static HANDLE g_hTimer = NULL;
static HANDLE g_hTimerQueue = NULL;
static DWORD g_ElapsedTime = 0;
static BOOL g_TimerIgnore = FALSE;
static DWORD g_Period = 1000;


static void
TerminateProcessById(DWORD dwProcessId)
{
    BOOL bTerminated = FALSE;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);
    if (hProcess) {
        bTerminated = TerminateProcess(hProcess, 3);
        CloseHandle(hProcess);
    }
    if (!bTerminated) {
       fprintf(stderr, "catchsegv: error: failed to interrupt target (0x%08lx)\n", GetLastError());
       exit(EXIT_FAILURE);
    }
}


/*
 * Periodically scans the desktop for modal dialog windows.
 *
 * See also http://msdn.microsoft.com/en-us/library/ms940840.aspx
 */
static BOOL CALLBACK
EnumWindowCallback(HWND hWnd, LPARAM lParam)
{
    DWORD dwProcessId = 0;
    DWORD dwThreadId;

    dwThreadId = GetWindowThreadProcessId(hWnd, &dwProcessId);
    if (dwProcessId == lParam) {
        if (GetWindowLong(hWnd, GWL_STYLE) & DS_MODALFRAME) {
            char szWindowText[256];
            if (GetWindowTextA(hWnd, szWindowText, _countof(szWindowText)) <= 0) {
                szWindowText[0] = 0;
            }

            fprintf(stderr, "catchsegv: error: message dialog detected (%s)\n", szWindowText);

            g_TimerIgnore = TRUE;

            assert(dwThreadId != 0);

            TrapThread(dwProcessId, dwThreadId);

            return FALSE;
        }
    }

    return TRUE;
}


static VOID CALLBACK
TimeOutCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    DWORD dwProcessId = (DWORD)(UINT_PTR)lpParam;

    if (g_TimerIgnore) {
        return;
    }

    EnumWindows(EnumWindowCallback, (LPARAM)dwProcessId);

    if (g_TimerIgnore) {
        return;
    }

    g_ElapsedTime += g_Period;

    if (!g_TimeOut || g_ElapsedTime < g_TimeOut*1000) {
        return;
    }

    fprintf(stderr, "catchsegv: time out (%lu sec) exceeded\n", g_TimeOut);

    g_TimerIgnore = TRUE;

    TerminateProcessById(dwProcessId);
}


static void
Usage(void)
{
    fputs("usage: catchsegv [options] <command-line>\n"
          "\n"
          "options:\n"
          "  -? displays command line help text\n"
          "  -v enables verbose output from the debugger\n"
          "  -t <seconds> specifies a timeout in seconds \n"
          "  -1 dump stack on first chance exceptions \n",
          stderr);
}


int
main(int argc, char** argv)
{
    DebugOptions debugOptions = {0};
    debugOptions.breakpoint_flag = 0;
    debugOptions.verbose_flag = 0;
    debugOptions.debug_flag = 0;
    debugOptions.first_chance = 0;

    /*
     * Disable error message boxes.
     */

#ifdef NDEBUG
    SetErrorMode(SEM_FAILCRITICALERRORS |
                 SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);

    // Disable assertion failure message box
    // http://msdn.microsoft.com/en-us/library/sas1dkb2.aspx
    _set_error_mode(_OUT_TO_STDERR);
#ifdef _MSC_VER
    // Disable abort message box
    // http://msdn.microsoft.com/en-us/library/e631wekh.aspx
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#endif

    /*
     * Parse command line arguments
     */

    while (1) {
        int opt = getopt(argc, argv, "?1dht:v");

        switch (opt) {
        case 'h':
            Usage();
            return 0;
        case 'v':
            debugOptions.verbose_flag = TRUE;
            break;
        case 'd':
            debugOptions.debug_flag = TRUE;
            break;
        case '1':
            debugOptions.first_chance = TRUE;
            break;
        case 't':
            g_TimeOut = strtoul(optarg, NULL, 0);
            break;
        case '?':
            if (optopt == '?') {
                Usage();
                return 0;
            }
            /* fall-trhough */
        default:
            opt = -1;
            break;
        }

        if (opt == -1) {
            break;
        }
    }

    /*
     * Concatenate remaining arguments into a command line
     */

    PSTR pCommandLine = g_CommandLine;

    while (optind < argc) {
        char *arg = argv[optind];
        ULONG length;
        BOOL quote;

        length = (ULONG)strlen(arg);
        if (length + 3 + (pCommandLine - g_CommandLine) >= sizeof g_CommandLine) {
            fprintf(stderr, "catchsegv: error: command line length exceeds %Iu characters\n", sizeof g_CommandLine);
            return 1;
        }

        quote = (strchr(arg, ' ') || strchr(arg, '\t'));

        if (quote) {
            *pCommandLine++ = '"';
        }

        memcpy(pCommandLine, arg, length + 1);
        pCommandLine += length;

        if (quote) {
            *pCommandLine++ = '"';
        }

        *pCommandLine++ = ' ';

        ++optind;
    }

    *pCommandLine = 0;

    if (strlen(g_CommandLine) == 0) {
        fprintf(stderr, "catchsegv: error: no command line given\n\n");
        Usage();
        return EXIT_FAILURE;
    }

    setDumpCallback(&outputCallback);

    // Disable debug heap
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366705.aspx
    SetEnvironmentVariableA("_NO_DEBUG_HEAP", "1");

    STARTUPINFOA StartupInfo;
    ZeroMemory(&StartupInfo, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION ProcessInformation;
    ZeroMemory(&ProcessInformation, sizeof ProcessInformation);

    if (!CreateProcessA(NULL, // lpApplicationName
                        g_CommandLine,
                        NULL, // lpProcessAttributes
                        NULL, // lpThreadAttributes
                        TRUE, // bInheritHandles
                        DEBUG_ONLY_THIS_PROCESS,
                        NULL, // lpEnvironment
                        NULL, // lpCurrentDirectory
                        &StartupInfo,
                        &ProcessInformation)) {
         fprintf(stderr, "catchsegv: error: failed to create the process (0x%08lx)\n", GetLastError());
         exit(EXIT_FAILURE);
    }

    DWORD dwProcessId = GetProcessId(ProcessInformation.hProcess);
    debugOptions.dwProcessId = dwProcessId;

    g_hTimerQueue = CreateTimerQueue();
    if (g_hTimerQueue == NULL) {
        fprintf(stderr, "catchsegv: error: failed to create a timer queue (0x%08lx)\n", GetLastError());
        return EXIT_FAILURE;
    }

    TIMECAPS tc;
    MMRESULT mmRes = timeGetDevCaps(&tc, sizeof tc);
    if (mmRes == MMSYSERR_NOERROR &&
        tc.wPeriodMax < g_Period) {
        g_Period = tc.wPeriodMax;
    }

    if (!CreateTimerQueueTimer(&g_hTimer, g_hTimerQueue,
                               (WAITORTIMERCALLBACK)TimeOutCallback,
                               (PVOID)(UINT_PTR)dwProcessId, g_Period, g_Period, 0)) {
        fprintf(stderr, "catchsegv: error: failed to CreateTimerQueueTimer failed (0x%08lx)\n", GetLastError());
        return EXIT_FAILURE;
    }

    /*
     * Set DbgHelp options
     */

    SetSymOptions(debugOptions.debug_flag);

    /*
     * Main event loop.
     */

    DebugMainLoop(&debugOptions);

    DWORD dwExitCode = STILL_ACTIVE;
    GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode);

    CloseHandle(ProcessInformation.hProcess);
    CloseHandle(ProcessInformation.hThread);

    return dwExitCode;
}
