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
#include <ntstatus.h>

#include <stdlib.h>

#include "debugger.h"
#include "log.h"
#include "dialog.h"
#include "errmsg.h"
#include "outdbg.h"
#include "symbols.h"


typedef struct {
    DWORD dwProcessId;
    HANDLE hProcess;
}
PROCESS_LIST_INFO, * PPROCESS_LIST_INFO;

typedef struct {
    DWORD dwProcessId;
    DWORD dwThreadId;
    HANDLE hThread;
    LPVOID lpThreadLocalBase;
    LPTHREAD_START_ROUTINE lpStartAddress;
}
THREAD_LIST_INFO, * PTHREAD_LIST_INFO;

typedef struct {
    DWORD dwProcessId;
    HANDLE hFile;
    LPVOID lpBaseAddress;
    DWORD dwDebugInfoFileOffset;
    DWORD nDebugInfoSize;
    LPVOID lpImageName;
    WORD fUnicode;
}
MODULE_LIST_INFO, * PMODULE_LIST_INFO;


static unsigned nProcesses = 0, maxProcesses = 0;
static PPROCESS_LIST_INFO ProcessListInfo = NULL;

static unsigned nThreads = 0, maxThreads = 0;
static PTHREAD_LIST_INFO ThreadListInfo = NULL;

static unsigned nModules = 0, maxModules = 0;
static PMODULE_LIST_INFO ModuleListInfo = NULL;


BOOL ObtainSeDebugPrivilege(void)
{
    HANDLE hToken;
    PTOKEN_PRIVILEGES NewPrivileges;
    BYTE OldPriv[1024];
    PBYTE pbOldPriv;
    ULONG cbNeeded;
    BOOLEAN fRc;
    LUID LuidPrivilege;

    // Make sure we have access to adjust and to get the old token privileges
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        OutputDebug("OpenProcessToken failed with %s\n", LastErrorMessage());

        return FALSE;
    }

    cbNeeded = 0;

    // Initialize the privilege adjustment structure
    LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &LuidPrivilege );

    NewPrivileges = (PTOKEN_PRIVILEGES) LocalAlloc(
        LMEM_ZEROINIT,
        sizeof(TOKEN_PRIVILEGES) + (1 - ANYSIZE_ARRAY)*sizeof(LUID_AND_ATTRIBUTES)
    );
    if(NewPrivileges == NULL)
    {
        OutputDebug("LocalAlloc failed with %s", LastErrorMessage());

        return FALSE;
    }

    NewPrivileges->PrivilegeCount = 1;
    NewPrivileges->Privileges[0].Luid = LuidPrivilege;
    NewPrivileges->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Enable the privilege
    pbOldPriv = OldPriv;
    fRc = AdjustTokenPrivileges(
        hToken,
        FALSE,
        NewPrivileges,
        1024,
        (PTOKEN_PRIVILEGES)pbOldPriv,
        &cbNeeded
    );

    if (!fRc) {

        // If the stack was too small to hold the privileges
        // then allocate off the heap
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {

            pbOldPriv = LocalAlloc(LMEM_FIXED, cbNeeded);
            if (pbOldPriv == NULL)
            {
                OutputDebug("LocalAlloc: %s", LastErrorMessage());
                return FALSE;
            }

            fRc = AdjustTokenPrivileges(
                hToken,
                FALSE,
                NewPrivileges,
                cbNeeded,
                (PTOKEN_PRIVILEGES)pbOldPriv,
                &cbNeeded
            );
        }
    }
    return fRc;
}

static BOOL TerminateProcessId(DWORD dwProcessId, UINT uExitCode)
{
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);
    if (hProcess == NULL)
        return FALSE;

    BOOL bRet = TerminateProcess(hProcess, uExitCode);

    CloseHandle(hProcess);

    return bRet;
}


static BOOL CALLBACK
symCallback(HANDLE hProcess,
            ULONG ActionCode,
            ULONG64 CallbackData,
            ULONG64 UserContext)
{
    if (ActionCode == CBA_DEBUG_INFO) {
        OutputDebug("%s", (LPCSTR)(UINT_PTR)CallbackData);
        return TRUE;
    }

    return FALSE;
}


BOOL DebugMainLoop(const DebugOptions *pOptions)
{
    BOOL fFinished = FALSE;
    BOOL fSignalled = FALSE;

    unsigned i, j;

#ifndef NDEBUG
    // http://msdn.microsoft.com/en-us/library/ms680687.aspx
    SetEnvironmentVariableA("DBGHELP_DBGOUT", "1");
#endif

    while(!fFinished)
    {
        DEBUG_EVENT DebugEvent;            // debugging event information
        DWORD dwContinueStatus = DBG_CONTINUE;    // exception continuation

        // Wait for a debugging event to occur. The second parameter indicates
        // that the function does not return until a debugging event occurs.
        if(!WaitForDebugEvent(&DebugEvent, INFINITE))
        {
            ErrorMessageBox(_T("WaitForDebugEvent: %s"), LastErrorMessage());

            return FALSE;
        }

        if (pOptions->debug_flag)
            OutputDebug("DEBUG_EVENT:\r\n");

        // Process the debugging event code.
        switch (DebugEvent.dwDebugEventCode)
        {
            case EXCEPTION_DEBUG_EVENT:
                ;

                PEXCEPTION_RECORD pExceptionRecord = &DebugEvent.u.Exception.ExceptionRecord;

                // Process the exception code. When handling
                // exceptions, remember to set the continuation
                // status parameter (dwContinueStatus). This value
                // is used by the ContinueDebugEvent function.
                if (pOptions->debug_flag) {
                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "EXCEPTION_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );
                    OutputDebug(
                        "\tExceptionCode = %lX\r\n\tExceptionFlags = %lX\r\n\tExceptionAddress = %p\r\n\tdwFirstChance = %lX\r\n",
                        pExceptionRecord->ExceptionCode,
                        pExceptionRecord->ExceptionFlags,
                        pExceptionRecord->ExceptionAddress,
                        DebugEvent.u.Exception.dwFirstChance
                    );
                }

                dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;

                BOOL bFirstBreakPoint = (pExceptionRecord->ExceptionCode == STATUS_BREAKPOINT ||
                                         pExceptionRecord->ExceptionCode == STATUS_WX86_BREAKPOINT) &&
                                         DebugEvent.u.Exception.dwFirstChance;

                if (bFirstBreakPoint) {
                    // Signal the aedebug event
                    if (pOptions->hEvent && !fSignalled) {
                        OutputDebug("SetEvent(%p)\n", pOptions->hEvent);
                        SetEvent(pOptions->hEvent);
                        CloseHandle(pOptions->hEvent);
                        fSignalled = TRUE;
                    }

                    dwContinueStatus = DBG_CONTINUE;

                    /*
                     * We ignore first-chance breakpoints by default.
                     *
                     * We get one of these whenever we attach to a process.
                     * But in some cases, we never get a second-chance, e.g.,
                     * when we're attached through MSVCRT's abort().
                     */
                    if (!pOptions->breakpoint_flag) {
                        break;
                    }
                }

                // Find the process in the process list
                PPROCESS_LIST_INFO pProcessInfo;
                PTHREAD_LIST_INFO pThreadInfo;
                assert(nProcesses);
                i = 0;
                while (i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId) {
                    ++i;
                }
                assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);
                pProcessInfo = &ProcessListInfo[i];

                SymRefreshModuleList(pProcessInfo->hProcess);

                dumpException(appendText,
                              pProcessInfo->hProcess,
                              &DebugEvent.u.Exception.ExceptionRecord);

                // Find the thread in the thread list
                assert(nThreads);
                for (i = 0; i < nThreads; ++i) {
                    assert(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId);
                    if (ThreadListInfo[i].dwThreadId != DebugEvent.dwThreadId &&
                        !bFirstBreakPoint &&
                        !pOptions->verbose_flag) {
                            continue;
                    }
                    pThreadInfo = &ThreadListInfo[i];

                    dumpStack(appendText,
                              pProcessInfo->hProcess,
                              pThreadInfo->hThread, NULL);
                }

                if (!DebugEvent.u.Exception.dwFirstChance) {
                    /*
                     * Terminate the process. As continuing would cause the JIT debugger
                     * to be invoked again.
                     */
                    TerminateProcessId(DebugEvent.dwProcessId,
                                       pExceptionRecord->ExceptionCode);
                }

                break;

            case CREATE_THREAD_DEBUG_EVENT:
                // As needed, examine or change the thread's registers
                // with the GetThreadContext and SetThreadContext functions;
                // and suspend and resume thread execution with the
                // SuspendThread and ResumeThread functions.
                if(pOptions->debug_flag)
                {
                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "CREATE_THREAD_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );
                    OutputDebug(
                        "\thThread = %p\r\n\tlpThreadLocalBase = %p\r\n\tlpStartAddress = %p\r\n",
                        DebugEvent.u.CreateThread.hThread,
                        DebugEvent.u.CreateThread.lpThreadLocalBase,
                        DebugEvent.u.CreateThread.lpStartAddress
                    );
                }

                // Add the thread to the thread list
                if(nThreads == maxThreads)
                    ThreadListInfo = (PTHREAD_LIST_INFO) (maxThreads ? realloc(ThreadListInfo, (maxThreads *= 2)*sizeof(THREAD_LIST_INFO)) : malloc((maxThreads = 4)*sizeof(THREAD_LIST_INFO)));
                i = nThreads++;
                while(i > 0 && (DebugEvent.dwProcessId < ThreadListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i - 1].dwProcessId && DebugEvent.dwThreadId < ThreadListInfo[i - 1].dwThreadId)))
                {
                    ThreadListInfo[i] = ThreadListInfo[i - 1];
                    --i;
                }
                ThreadListInfo[i].dwProcessId = DebugEvent.dwProcessId;
                ThreadListInfo[i].dwThreadId = DebugEvent.dwThreadId;
                ThreadListInfo[i].hThread = DebugEvent.u.CreateThread.hThread;
                ThreadListInfo[i].lpThreadLocalBase = DebugEvent.u.CreateThread.lpThreadLocalBase;
                ThreadListInfo[i].lpStartAddress = DebugEvent.u.CreateThread.lpStartAddress;
                break;

            case CREATE_PROCESS_DEBUG_EVENT:
                ;
                HANDLE hProcess = DebugEvent.u.CreateProcessInfo.hProcess;

                // As needed, examine or change the registers of the
                // process's initial thread with the GetThreadContext and
                // SetThreadContext functions; read from and write to the
                // process's virtual memory with the ReadProcessMemory and
                // WriteProcessMemory functions; and suspend and resume
                // thread execution with the SuspendThread and ResumeThread
                // functions.
                if(pOptions->debug_flag)
                {
                    TCHAR szBuffer[MAX_PATH];
                    LPVOID lpImageName;

                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "CREATE_PROCESS_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );

                    if(!ReadProcessMemory(hProcess, DebugEvent.u.CreateProcessInfo.lpImageName, &lpImageName, sizeof(LPVOID), NULL) ||
                        !ReadProcessMemory(hProcess, lpImageName, szBuffer, sizeof(szBuffer), NULL))
                        lstrcpyn(szBuffer, "NULL", sizeof(szBuffer));

                    OutputDebug(
                        "\thFile = %p\r\n\thProcess = %p\r\n\thThread = %p\r\n\tlpBaseOfImage = %p\r\n\tdwDebugInfoFileOffset = %lX\r\n\tnDebugInfoSize = %lX\r\n\tlpThreadLocalBase = %p\r\n\tlpStartAddress = %p\r\n\tlpImageName = %s\r\n\tfUnicoded = %X\r\n",
                        DebugEvent.u.CreateProcessInfo.hFile,
                        hProcess,
                        DebugEvent.u.CreateProcessInfo.hThread,
                        DebugEvent.u.CreateProcessInfo.lpBaseOfImage,
                        DebugEvent.u.CreateProcessInfo.dwDebugInfoFileOffset,
                        DebugEvent.u.CreateProcessInfo.nDebugInfoSize,
                        DebugEvent.u.CreateProcessInfo.lpThreadLocalBase,
                        DebugEvent.u.CreateProcessInfo.lpStartAddress,
                        szBuffer,
                        DebugEvent.u.CreateProcessInfo.fUnicode
                    );
                }

                // Add the process to the process list
                if(nProcesses == maxProcesses)
                    ProcessListInfo = (PPROCESS_LIST_INFO) (maxProcesses ? realloc(ThreadListInfo, (maxProcesses *= 2)*sizeof(PROCESS_LIST_INFO)) : malloc((maxProcesses = 4)*sizeof(PROCESS_LIST_INFO)));
                i = nProcesses++;
                while(i > 0 && DebugEvent.dwProcessId < ProcessListInfo[i - 1].dwProcessId)
                {
                    ProcessListInfo[i] = ProcessListInfo[i - 1];
                    --i;
                }
                ProcessListInfo[i].dwProcessId = DebugEvent.dwProcessId;
                ProcessListInfo[i].hProcess = hProcess;

                // Add the initial thread of the process to the thread list
                if(nThreads == maxThreads)
                    ThreadListInfo = (PTHREAD_LIST_INFO) (maxThreads ? realloc(ThreadListInfo, (maxThreads *= 2)*sizeof(THREAD_LIST_INFO)) : malloc((maxThreads = 4)*sizeof(THREAD_LIST_INFO)));
                i = nThreads++;
                while(i > 0 && (DebugEvent.dwProcessId < ThreadListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i - 1].dwProcessId && DebugEvent.dwThreadId < ThreadListInfo[i - 1].dwThreadId)))
                {
                    ThreadListInfo[i] = ThreadListInfo[i - 1];
                    --i;
                }
                ThreadListInfo[i].dwProcessId = DebugEvent.dwProcessId;
                ThreadListInfo[i].dwThreadId = DebugEvent.dwThreadId;
                ThreadListInfo[i].hThread = DebugEvent.u.CreateProcessInfo.hThread;
                ThreadListInfo[i].lpThreadLocalBase = DebugEvent.u.CreateProcessInfo.lpThreadLocalBase;
                ThreadListInfo[i].lpStartAddress = DebugEvent.u.CreateProcessInfo.lpStartAddress;

                // Add the initial module of the process to the module list
                if(nModules == maxModules)
                    ModuleListInfo = (PMODULE_LIST_INFO) (maxModules ? realloc(ModuleListInfo, (maxModules *= 2)*sizeof(MODULE_LIST_INFO)) : malloc((maxModules = 4)*sizeof(MODULE_LIST_INFO)));
                i = nModules++;
                while(i > 0 && (DebugEvent.dwProcessId < ModuleListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ModuleListInfo[i - 1].dwProcessId && DebugEvent.u.CreateProcessInfo.lpBaseOfImage < ModuleListInfo[i - 1].lpBaseAddress)))
                {
                    ModuleListInfo[i] = ModuleListInfo[i - 1];
                    --i;
                }
                ModuleListInfo[i].dwProcessId = DebugEvent.dwProcessId;
                ModuleListInfo[i].hFile = DebugEvent.u.CreateProcessInfo.hFile;
                ModuleListInfo[i].lpBaseAddress = DebugEvent.u.CreateProcessInfo.lpBaseOfImage;
                ModuleListInfo[i].dwDebugInfoFileOffset = DebugEvent.u.CreateProcessInfo.dwDebugInfoFileOffset;
                ModuleListInfo[i].nDebugInfoSize = DebugEvent.u.CreateProcessInfo.nDebugInfoSize;
                ModuleListInfo[i].lpImageName = DebugEvent.u.CreateProcessInfo.lpImageName;
                ModuleListInfo[i].fUnicode = DebugEvent.u.CreateProcessInfo.fUnicode;

                assert(!bSymInitialized);

                // Provide default symbol search path
                // http://msdn.microsoft.com/en-gb/library/windows/hardware/ff558829.aspx
                char szSymSearchPathBuf[512];
                const char *szSymSearchPath = NULL;
                if (getenv("_NT_SYMBOL_PATH") == NULL &&
                    getenv("_NT_ALTERNATE_SYMBOL_PATH") == NULL) {
                    const char *szLocalAppData = getenv("LOCALAPPDATA");
                    assert(szLocalAppData != NULL);
                    _snprintf(szSymSearchPathBuf,
                              sizeof szSymSearchPathBuf,
                              "srv*%s\\drmingw*http://msdl.microsoft.com/download/symbols",
                              szLocalAppData);
                    szSymSearchPath = szSymSearchPathBuf;
                }

                DWORD dwSymOptions = SymGetOptions();
                dwSymOptions |=
                    SYMOPT_LOAD_LINES |
                    SYMOPT_DEFERRED_LOADS;
                if (pOptions->debug_flag)
                    dwSymOptions |= SYMOPT_DEBUG;

#ifdef _WIN64
                BOOL bWow64 = FALSE;
                IsWow64Process(hProcess, &bWow64);
                if (bWow64) {
                    dwSymOptions |= SYMOPT_INCLUDE_32BIT_MODULES;
                }
#endif

                SymSetOptions(dwSymOptions);
                if (!SymInitialize(hProcess, szSymSearchPath, FALSE)) {
                    ErrorMessageBox(_T("SymInitialize: %s"), LastErrorMessage());
                }

                SymRegisterCallback64(hProcess, &symCallback, 0);

                bSymInitialized = TRUE;

                break;

            case EXIT_THREAD_DEBUG_EVENT:
                // Display the thread's exit code.
                if(pOptions->debug_flag)
                {
                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "EXIT_THREAD_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );
                    OutputDebug(
                        "\tdwExitCode = %lX\r\n",
                        DebugEvent.u.ExitThread.dwExitCode
                    );
                }

                // Remove the thread from the thread list
                assert(nThreads);
                i = 0;
                while(i < nThreads && (DebugEvent.dwProcessId > ThreadListInfo[i].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i].dwProcessId && DebugEvent.dwThreadId > ThreadListInfo[i].dwThreadId)))
                    ++i;
                assert(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId && ThreadListInfo[i].dwThreadId == DebugEvent.dwThreadId);
                while(++i < nThreads)
                    ThreadListInfo[i - 1] = ThreadListInfo[i];
                --nThreads;
                break;

            case EXIT_PROCESS_DEBUG_EVENT:
                // Display the process's exit code.
                if(pOptions->debug_flag)
                {
                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "EXIT_PROCESS_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );
                    OutputDebug(
                        "\tdwExitCode = %lX\r\n",
                        DebugEvent.u.ExitProcess.dwExitCode
                    );
                }

                // Remove all threads of the process from the thread list
                for(i = j = 0; i < nThreads; ++i)
                {
                    if(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId)
                    {
                    }
                    else
                        ++j;

                    if(j != i)
                        ThreadListInfo[j] = ThreadListInfo[i];
                }
                nThreads = j;

                // Remove all modules of the process from the module list
                for(i = j = 0; i < nModules; ++i)
                {
                    if(ModuleListInfo[i].dwProcessId != DebugEvent.dwProcessId)
                        ++j;
                    if(j != i)
                        ModuleListInfo[j] = ModuleListInfo[i];
                }
                nModules = j;

                // Remove the process from the process list
                assert(nProcesses);
                i = 0;
                while(i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId)
                    ++i;
                assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);

                if (bSymInitialized) {
                    if (!SymCleanup(ProcessListInfo[i].hProcess))
                        assert(0);

                    bSymInitialized = FALSE;
                }

                while(++i < nProcesses)
                    ProcessListInfo[i - 1] = ProcessListInfo[i];

                if(!--nProcesses)
                {
                    assert(!nThreads && !nModules);
                    fFinished = TRUE;
                }

                break;

            case LOAD_DLL_DEBUG_EVENT:
                // Read the debugging information included in the newly
                // loaded DLL.
                if(pOptions->debug_flag)
                {
                    TCHAR szBuffer[MAX_PATH];
                    LPVOID lpImageName;

                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "LOAD_DLL_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );

                    if(!ReadProcessMemory(DebugEvent.u.CreateProcessInfo.hProcess, DebugEvent.u.CreateProcessInfo.lpImageName, &lpImageName, sizeof(LPVOID), NULL) ||
                        !ReadProcessMemory(DebugEvent.u.CreateProcessInfo.hProcess, lpImageName, szBuffer, sizeof(szBuffer), NULL))
                        lstrcpyn(szBuffer, "NULL", sizeof szBuffer);

                    OutputDebug(
                        "\thFile = %p\r\n\tlpBaseOfDll = %p\r\n\tdwDebugInfoFileOffset = %lX\r\n\tnDebugInfoSize = %lX\r\n\tlpImageName = %s\r\n\tfUnicoded = %X\r\n",
                        DebugEvent.u.LoadDll.hFile,
                        DebugEvent.u.LoadDll.lpBaseOfDll,
                        DebugEvent.u.LoadDll.dwDebugInfoFileOffset,
                        DebugEvent.u.LoadDll.nDebugInfoSize,
                        szBuffer,
                        DebugEvent.u.LoadDll.fUnicode
                    );
                }

                // Add the module to the module list
                if(nModules == maxModules)
                    ModuleListInfo = (PMODULE_LIST_INFO) (maxModules ? realloc(ModuleListInfo, (maxModules *= 2)*sizeof(MODULE_LIST_INFO)) : malloc((maxModules = 4)*sizeof(MODULE_LIST_INFO)));
                i = nModules++;
                while(i > 0 && (DebugEvent.dwProcessId < ModuleListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ModuleListInfo[i - 1].dwProcessId && DebugEvent.u.LoadDll.lpBaseOfDll < ModuleListInfo[i - 1].lpBaseAddress)))
                {
                    ModuleListInfo[i] = ModuleListInfo[i - 1];
                    --i;
                }
                ModuleListInfo[i].dwProcessId = DebugEvent.dwProcessId;
                ModuleListInfo[i].hFile = DebugEvent.u.LoadDll.hFile;
                ModuleListInfo[i].lpBaseAddress = DebugEvent.u.LoadDll.lpBaseOfDll;
                ModuleListInfo[i].dwDebugInfoFileOffset = DebugEvent.u.LoadDll.dwDebugInfoFileOffset;
                ModuleListInfo[i].nDebugInfoSize = DebugEvent.u.LoadDll.nDebugInfoSize;
                ModuleListInfo[i].lpImageName = DebugEvent.u.LoadDll.lpImageName;
                ModuleListInfo[i].fUnicode = DebugEvent.u.LoadDll.fUnicode;

                break;

            case UNLOAD_DLL_DEBUG_EVENT:
                // Display a message that the DLL has been unloaded.
                if(pOptions->debug_flag)
                {
                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "UNLOAD_DLL_DEBUG_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );
                    OutputDebug(
                        "\tlpBaseOfDll = %p\r\n",
                        DebugEvent.u.UnloadDll.lpBaseOfDll
                    );
                }

                // Remove the module from the module list
                assert(nModules);
                i = 0;
                while(i < nModules && (DebugEvent.dwProcessId > ModuleListInfo[i].dwProcessId || (DebugEvent.dwProcessId == ModuleListInfo[i].dwProcessId && DebugEvent.u.UnloadDll.lpBaseOfDll > ModuleListInfo[i].lpBaseAddress)))
                    ++i;
                assert(ModuleListInfo[i].dwProcessId == DebugEvent.dwProcessId && ModuleListInfo[i].lpBaseAddress == DebugEvent.u.UnloadDll.lpBaseOfDll);

                while(++i < nModules)
                    ModuleListInfo[i - 1] = ModuleListInfo[i];
                --nModules;
                assert(nModules);
                break;

            case OUTPUT_DEBUG_STRING_EVENT:
                // Display the output debugging string.
                if(pOptions->debug_flag)
                {
                    OutputDebug(
                        "\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        "OUTPUT_DEBUG_STRING_EVENT",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );
                    /* XXX: Must use ReadProcessMemory */
#if 0
                    OutputDebug(
                        "\tlpDebugStringData = %s\r\n",
                        DebugEvent.u.DebugString.lpDebugStringData
                    );
#endif
                }
                break;

             default:
                if(pOptions->debug_flag)
                    OutputDebug(
                        "\tdwDebugEventCode = 0x%lX\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n",
                        DebugEvent.dwDebugEventCode,
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                    );

        }

        // Resume executing the thread that reported the debugging event.
        ContinueDebugEvent(
            DebugEvent.dwProcessId,
            DebugEvent.dwThreadId,
            dwContinueStatus
        );
    }

    appendText(_T("--\r\n"));

    return TRUE;
}
