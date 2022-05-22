/*
 * Copyright 2009-2018 Jose Fonseca
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

#include <string>

#include "log.h"
#include "debugger.h"
#include "symbols.h"
#include "getoptW.h"


static void
outputCallback(const char *s)
{
    fputs(s, stderr);
    fflush(stderr);
}


static ULONG g_TimeOut = 0;
static HANDLE g_hTimer = NULL;
static HANDLE g_hTimerQueue = NULL;
static DWORD g_ElapsedTime = 0;
static BOOL g_TimerIgnore = FALSE;
static DWORD g_Period = 1000;
static BOOL g_ModalDialogIgnore = FALSE;


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
    if (dwProcessId == (DWORD)lParam) {
        if (GetWindowLong(hWnd, GWL_STYLE) & DS_MODALFRAME) {
            wchar_t szWindowText[256];
            if (GetWindowTextW(hWnd, szWindowText, _countof(szWindowText)) <= 0) {
                szWindowText[0] = 0;
            }

            fprintf(stderr, "catchsegv: error: message dialog detected (%ls)\n", szWindowText);

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

    if (!g_ModalDialogIgnore) {
        EnumWindows(EnumWindowCallback, (LPARAM)dwProcessId);
    }

    if (g_TimerIgnore) {
        return;
    }

    g_ElapsedTime += g_Period;

    if (!g_TimeOut || g_ElapsedTime < g_TimeOut * 1000) {
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
          "  -?|-h        displays command line help text\n"
          "  -v           enables verbose output from the debugger\n"
          "  -d           enables debugging output (for debugging catchsegv itself)\n"
          "  -t SECONDS   specifies a timeout in seconds\n"
          "  -1           dump stack on first chance exceptions\n"
          "  -m           ignore modal dialogs\n"
          "  -z           write minidumps\n"
          "  -Z DIRECTORY write minidumps to specified directory\n"
          "  -H           use debug heap\n",
          stderr);
}


/*
 * Ignore Ctrl-C / Ctrl-Break events, so this process stays alive long enough
 * to dump the stack backtraces of the debuggee.  But honour the second event.
 */
static BOOL WINAPI
consoleCtrlHandler(DWORD fdwCtrlType)
{
    static int cCtrlC = 0;
    static int cCtrlBreak = 0;

    switch (fdwCtrlType) {
    case CTRL_C_EVENT:
        fprintf(stderr, "catchsegv: warning: caught Ctrl-C event\n");
        return cCtrlC++ ? FALSE : TRUE;
    case CTRL_BREAK_EVENT:
        fprintf(stderr, "catchsegv: warning: caught Ctrl-Break event\n");
        return cCtrlBreak++ ? FALSE : TRUE;
    default:
        return FALSE;
    }
}


/**
 * Determine whether an argument should be quoted.
 */
static bool
needsQuote(const wchar_t *arg)
{
    wchar_t c;
    while (true) {
        c = *arg++;
        if (c == L'\0') {
            break;
        }
        if (c == L' ' || c == L'\t' || c == L'\"') {
            return true;
        }
        if (c == L'\\') {
            c = *arg++;
            if (c == L'\0') {
                break;
            }
            if (c == L'"') {
                return true;
            }
        }
    }
    return false;
}


static void
quoteArg(std::wstring &s, const wchar_t *arg)
{
    wchar_t c;
    unsigned backslashes = 0;

    s.push_back(L'"');
    while (true) {
        c = *arg++;
        if (c == L'\0') {
            break;
        } else if (c == L'"') {
            while (backslashes) {
                s.push_back(L'\\');
                --backslashes;
            }
            s.push_back(L'\\');
        } else {
            if (c == L'\\') {
                ++backslashes;
            } else {
                backslashes = 0;
            }
        }
        s.push_back(c);
    }
    s.push_back(L'"');
}


int
wmain(int argc, wchar_t **argv)
{
    /*
     * Disable error message boxes.
     */

#ifdef NDEBUG
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

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

    bool debugHeap = false;
    while (1) {
        int opt = getoptW(argc, argv, L"?1dhHmt:zZ:v");

        switch (opt) {
        case L'h':
            Usage();
            return 0;
        case L'v':
            debugOptions.verbose_flag = true;
            break;
        case L'd':
            debugOptions.debug_flag = true;
            break;
        case L'1':
            debugOptions.first_chance = true;
            break;
        case L'm':
            g_ModalDialogIgnore = TRUE;
            break;
        case L't':
            g_TimeOut = wcstoul(optarg, NULL, 0);
            break;
        case L'H':
            debugHeap = true;
            break;
        case L'z':
            debugOptions.minidump = true;
            break;
        case L'Z':
            debugOptions.minidump = true;
            debugOptions.minidumpDir = optarg;
            break;
        case L'?':
            if (optopt == L'?') {
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

    std::wstring commandLine;

    char sep = 0;
    while (optind < argc) {
        const wchar_t *arg = argv[optind];

        if (sep) {
            commandLine.push_back(sep);
        }

        if (needsQuote(arg)) {
            quoteArg(commandLine, arg);
        } else {
            commandLine.append(arg);
        }

        sep = ' ';

        ++optind;
    }

    if (commandLine.empty()) {
        fprintf(stderr, "catchsegv: error: no command line given\n\n");
        Usage();
        return EXIT_FAILURE;
    }

    setDumpCallback(&outputCallback);

    SetConsoleCtrlHandler(&consoleCtrlHandler, TRUE);

    // Disable debug heap
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa366705.aspx
    if (!debugHeap) {
        SetEnvironmentVariableW(L"_NO_DEBUG_HEAP", L"1");
    }

    STARTUPINFOW StartupInfo;
    ZeroMemory(&StartupInfo, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION ProcessInformation;
    ZeroMemory(&ProcessInformation, sizeof ProcessInformation);

    if (!CreateProcessW(NULL, // lpApplicationName
                        const_cast<wchar_t *>(commandLine.c_str()),
                        NULL, // lpProcessAttributes
                        NULL, // lpThreadAttributes
                        TRUE, // bInheritHandles
                        DEBUG_PROCESS,
                        NULL, // lpEnvironment
                        NULL, // lpCurrentDirectory
                        &StartupInfo, &ProcessInformation)) {
        fprintf(stderr, "catchsegv: error: failed to create the process (0x%08lx)\n",
                GetLastError());
        exit(EXIT_FAILURE);
    }

    DWORD dwProcessId = GetProcessId(ProcessInformation.hProcess);

    g_hTimerQueue = CreateTimerQueue();
    if (g_hTimerQueue == NULL) {
        fprintf(stderr, "catchsegv: error: failed to create a timer queue (0x%08lx)\n",
                GetLastError());
        return EXIT_FAILURE;
    }

    TIMECAPS tc;
    MMRESULT mmRes = timeGetDevCaps(&tc, sizeof tc);
    if (mmRes == MMSYSERR_NOERROR && tc.wPeriodMax < g_Period) {
        g_Period = tc.wPeriodMax;
    }

    if (!CreateTimerQueueTimer(&g_hTimer, g_hTimerQueue, (WAITORTIMERCALLBACK)TimeOutCallback,
                               (PVOID)(UINT_PTR)dwProcessId, g_Period, g_Period, 0)) {
        fprintf(stderr, "catchsegv: error: failed to CreateTimerQueueTimer failed (0x%08lx)\n",
                GetLastError());
        return EXIT_FAILURE;
    }

    /*
     * Set DbgHelp options
     */

    SetSymOptions(debugOptions.debug_flag);

    /*
     * Main event loop.
     */

    DebugMainLoop();

    DWORD dwExitCode = STILL_ACTIVE;
    GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode);

    CloseHandle(ProcessInformation.hProcess);
    CloseHandle(ProcessInformation.hThread);

    return dwExitCode;
}
