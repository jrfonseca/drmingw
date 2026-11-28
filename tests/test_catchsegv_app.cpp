/*
 * Copyright 2026 Jose Fonseca
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
 * Debuggee for test_catchsegv.py, whose behavior is selected by the first
 * argument.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>


static int *volatile g_pNull = nullptr;


static __attribute__((noinline)) void
FaultingFunction(void)
{
    *g_pNull = 0;
}


// Crash, after printing the IDs and address that the minidump is expected to
// contain.
static int
crash(void)
{
    printf("pid=%lu\n", GetCurrentProcessId());
    printf("tid=%lu\n", GetCurrentThreadId());
    printf("func=0x%llx\n", (unsigned long long)(UINT_PTR)&FaultingFunction);
    fflush(stdout);

    FaultingFunction();

    return EXIT_FAILURE;
}


// Echo the arguments, to verify how catchsegv quotes them.
static int
args(int argc, char **argv)
{
    for (int i = 2; i < argc; ++i) {
        printf("arg=<%s>\n", argv[i]);
    }
    fflush(stdout);
    return EXIT_SUCCESS;
}


static int
outputDebugString(void)
{
    OutputDebugStringA("test_catchsegv_app: OutputDebugString marker\n");
    return EXIT_SUCCESS;
}


#define FIRST_CHANCE_EXCEPTION_CODE 0xE0000001


static LONG CALLBACK
vectoredHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == FIRST_CHANCE_EXCEPTION_CODE) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


static __attribute__((noinline)) void
RaiseFirstChanceException(void)
{
    RaiseException(FIRST_CHANCE_EXCEPTION_CODE, 0, 0, NULL);
}


// Raise an exception which is handled, so the debugger only ever sees it as
// first chance.
static int
firstChance(void)
{
    AddVectoredExceptionHandler(1, vectoredHandler);
    RaiseFirstChanceException();
    printf("survived first chance exception\n");
    fflush(stdout);
    return EXIT_SUCCESS;
}


static int
env(void)
{
    char szValue[256];
    DWORD dwRet = GetEnvironmentVariableA("_NO_DEBUG_HEAP", szValue, sizeof szValue);
    printf("_NO_DEBUG_HEAP=%s\n", dwRet && dwRet < sizeof szValue ? szValue : "(unset)");
    fflush(stdout);
    return EXIT_SUCCESS;
}


static int
messageBox(void)
{
    MessageBoxA(NULL, "test_catchsegv_app", "test_catchsegv_app", MB_OK);
    return EXIT_SUCCESS;
}


// Spawn a copy of this program which sleeps forever, then either sleep
// forever too or exit, depending on the given argument.
static int
spawnChild(const char *szThen)
{
    char szModule[MAX_PATH];
    GetModuleFileNameA(NULL, szModule, sizeof szModule);

    char szCommand[MAX_PATH + 32];
    snprintf(szCommand, sizeof szCommand, "\"%s\" sleep", szModule);

    STARTUPINFOA si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "error: failed to spawn child (%lu)\n", GetLastError());
        return EXIT_FAILURE;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    printf("child_pid=%lu\n", pi.dwProcessId);
    fflush(stdout);

    if (strcmp(szThen, "exit") == 0) {
        return EXIT_SUCCESS;
    }

    Sleep(INFINITE);
    return EXIT_SUCCESS;
}


// Report whether the process with the given ID is still running.
static int
checkPid(const char *szPid)
{
    DWORD dwProcessId = strtoul(szPid, NULL, 0);
    bool bRunning = false;
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, dwProcessId);
    if (hProcess) {
        bRunning = WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT;
        CloseHandle(hProcess);
    }
    printf("%s\n", bRunning ? "running" : "exited");
    fflush(stdout);
    return EXIT_SUCCESS;
}


// Create a window and wait to be killed.  Used to start Wine's desktop
// process (explorer.exe) outside catchsegv's debugging tree.
static int
window(void)
{
    HWND hWnd = CreateWindowA("STATIC", "test_catchsegv_app", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, NULL, NULL);
    if (!hWnd) {
        return EXIT_FAILURE;
    }
    printf("ready\n");
    fflush(stdout);
    Sleep(INFINITE);
    return EXIT_SUCCESS;
}


int
main(int argc, char **argv)
{
    if (argc >= 2) {
        const char *szMode = argv[1];
        if (strcmp(szMode, "crash") == 0) {
            return crash();
        }
        if (strcmp(szMode, "args") == 0) {
            return args(argc, argv);
        }
        if (strcmp(szMode, "output_debug_string") == 0) {
            return outputDebugString();
        }
        if (strcmp(szMode, "first_chance") == 0) {
            return firstChance();
        }
        if (strcmp(szMode, "env") == 0) {
            return env();
        }
        if (strcmp(szMode, "message_box") == 0) {
            return messageBox();
        }
        if (strcmp(szMode, "window") == 0) {
            return window();
        }
        if (strcmp(szMode, "sleep") == 0) {
            Sleep(INFINITE);
            return EXIT_SUCCESS;
        }
        if (strcmp(szMode, "spawn_child") == 0 && argc == 3) {
            return spawnChild(argv[2]);
        }
        if (strcmp(szMode, "check_pid") == 0 && argc == 3) {
            return checkPid(argv[2]);
        }
    }

    fprintf(stderr,
            "usage: %s crash|args|output_debug_string|first_chance|env|message_box|window|sleep\n"
            "       %s spawn_child sleep|exit\n"
            "       %s check_pid <pid>\n",
            argv[0], argv[0], argv[0]);
    return EXIT_FAILURE;
}
