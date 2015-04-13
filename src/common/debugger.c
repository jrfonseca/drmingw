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
        OutputDebug("OpenProcessToken failed with 0x%08lx\n", GetLastError());

        return FALSE;
    }

    cbNeeded = 0;

    // Initialize the privilege adjustment structure
    LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &LuidPrivilege );

    NewPrivileges = (PTOKEN_PRIVILEGES) LocalAlloc(
        LMEM_ZEROINIT,
        sizeof(TOKEN_PRIVILEGES) + (1 - ANYSIZE_ARRAY)*sizeof(LUID_AND_ATTRIBUTES)
    );
    if(NewPrivileges == NULL) {
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
            if (pbOldPriv == NULL) {
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


static BOOL CALLBACK
symCallback(HANDLE hProcess,
            ULONG ActionCode,
            ULONG64 CallbackData,
            ULONG64 UserContext)
{
    if (ActionCode == CBA_DEBUG_INFO) {
        lprintf("%s", (LPCSTR)(UINT_PTR)CallbackData);
        return TRUE;
    }

    return FALSE;
}


static char *
readProcessString(HANDLE hProcess, LPCVOID lpBaseAddress, SIZE_T nSize)
{
    LPSTR lpszBuffer = malloc(nSize + 1);
    SIZE_T NumberOfBytesRead = 0;

    if (!ReadProcessMemory(hProcess, lpBaseAddress, lpszBuffer, nSize, &NumberOfBytesRead)) {
        lpszBuffer[0] = '\0';
    }

    assert(NumberOfBytesRead <= nSize);
    lpszBuffer[NumberOfBytesRead] = '\0';
    return lpszBuffer;
}


BOOL DebugMainLoop(const DebugOptions *pOptions)
{
    BOOL fFinished = FALSE;
    BOOL fBreakpointSignalled = FALSE;
    BOOL fWowBreakpointSignalled = FALSE;
    unsigned i, j;

    while(!fFinished)
    {
        DEBUG_EVENT DebugEvent;            // debugging event information
        DWORD dwContinueStatus = DBG_CONTINUE;    // exception continuation
        PPROCESS_LIST_INFO pProcessInfo;
        PTHREAD_LIST_INFO pThreadInfo;

        // Wait for a debugging event to occur. The second parameter indicates
        // that the function does not return until a debugging event occurs.
        if(!WaitForDebugEvent(&DebugEvent, INFINITE))
        {
            OutputDebug("WaitForDebugEvent: 0x%08lx", GetLastError());

            return FALSE;
        }

        // Process the debugging event code.
        switch (DebugEvent.dwDebugEventCode) {
        case EXCEPTION_DEBUG_EVENT:
            ;

            PEXCEPTION_RECORD pExceptionRecord = &DebugEvent.u.Exception.ExceptionRecord;

            // Process the exception code. When handling
            // exceptions, remember to set the continuation
            // status parameter (dwContinueStatus). This value
            // is used by the ContinueDebugEvent function.
            if (pOptions->debug_flag) {
                lprintf("EXCEPTION PID=%lu TID=%lu ExceptionCode=0x%lx dwFirstChance=%lu\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        pExceptionRecord->ExceptionCode,
                        DebugEvent.u.Exception.dwFirstChance
                );
            }

            dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;

            if (DebugEvent.u.Exception.dwFirstChance) {
                if (pExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) {
                    // Signal the aedebug event
                    if (!fBreakpointSignalled) {
                        fBreakpointSignalled = TRUE;

                        if (pOptions->hEvent) {
                            OutputDebug("SetEvent(%p)\n", pOptions->hEvent);
                            SetEvent(pOptions->hEvent);
                            CloseHandle(pOptions->hEvent);
                        }

                        /*
                         * We ignore first-chance breakpoints by default.
                         *
                         * We get one of these whenever we attach to a process.
                         * But in some cases, we never get a second-chance, e.g.,
                         * when we're attached through MSVCRT's abort().
                         */
                        if (!pOptions->breakpoint_flag) {
                            dwContinueStatus = DBG_CONTINUE;
                            break;
                        }
                    }
                }

                if (pExceptionRecord->ExceptionCode == STATUS_WX86_BREAKPOINT) {
                    if (!fWowBreakpointSignalled) {
                        fWowBreakpointSignalled = TRUE;
                        dwContinueStatus = DBG_CONTINUE;
                    }
                }

                if (!pOptions->first_chance) {
                    break;
                }
            }

            // Find the process in the process list
            assert(nProcesses);
            i = 0;
            while (i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId) {
                ++i;
            }
            assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);
            pProcessInfo = &ProcessListInfo[i];

            SymRefreshModuleList(pProcessInfo->hProcess);

            dumpException(pProcessInfo->hProcess,
                          &DebugEvent.u.Exception.ExceptionRecord);

            // Find the thread in the thread list
            assert(nThreads);
            for (i = 0; i < nThreads; ++i) {
                assert(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId);
                if (ThreadListInfo[i].dwThreadId != DebugEvent.dwThreadId &&
                    pExceptionRecord->ExceptionCode != STATUS_BREAKPOINT &&
                    !pOptions->verbose_flag) {
                        continue;
                }
                pThreadInfo = &ThreadListInfo[i];

                dumpStack(pProcessInfo->hProcess,
                          pThreadInfo->hThread, NULL);
            }

            if (!DebugEvent.u.Exception.dwFirstChance) {
                /*
                 * Terminate the process. As continuing would cause the JIT debugger
                 * to be invoked again.
                 */
                TerminateProcess(pProcessInfo->hProcess,
                                 pExceptionRecord->ExceptionCode);
            }

            break;

        case CREATE_THREAD_DEBUG_EVENT:
            if (pOptions->debug_flag) {
                lprintf("CREATE_THREAD PID=%lu TID=%lu\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
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
            if (pOptions->debug_flag) {
                lprintf("CREATE_PROCESS PID=%lu TID=%lu\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                );
            }

            HANDLE hProcess = DebugEvent.u.CreateProcessInfo.hProcess;

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

            assert(!bSymInitialized);

            DWORD dwSymOptions = SymGetOptions();
            dwSymOptions |=
                SYMOPT_LOAD_LINES |
                SYMOPT_DEFERRED_LOADS;
            if (pOptions->debug_flag) {
                //dwSymOptions |= SYMOPT_DEBUG;
            }

#ifdef _WIN64
            BOOL bWow64 = FALSE;
            IsWow64Process(hProcess, &bWow64);
            if (bWow64) {
                dwSymOptions |= SYMOPT_INCLUDE_32BIT_MODULES;
            }
#endif

            SymSetOptions(dwSymOptions);
            if (!InitializeSym(hProcess, FALSE)) {
                OutputDebug("SymInitialize failed: 0x%08lx", GetLastError());
            }

            SymRegisterCallback64(hProcess, &symCallback, 0);

            bSymInitialized = TRUE;

            break;

        case EXIT_THREAD_DEBUG_EVENT:
            if (pOptions->debug_flag) {
                lprintf("EXIT_THREAD PID=%lu TID=%lu dwExitCode=0x%lx\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
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
            if (pOptions->debug_flag) {
                lprintf("EXIT_PROCESS PID=%lu TID=%lu dwExitCode=0x%lx\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
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
                assert(!nThreads);
                fFinished = TRUE;
            }

            break;

        case LOAD_DLL_DEBUG_EVENT:
            if (pOptions->debug_flag) {
                lprintf("LOAD_DLL PID=%lu TID=%lu lpBaseOfDll=%p\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.LoadDll.lpBaseOfDll
                );
            }

            break;

        case UNLOAD_DLL_DEBUG_EVENT:
            if (pOptions->debug_flag) {
                lprintf("UNLOAD_DLL PID=%lu TID=%lu lpBaseOfDll=%p\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.UnloadDll.lpBaseOfDll
                );
            }
            break;

        case OUTPUT_DEBUG_STRING_EVENT:
            if (pOptions->debug_flag) {
                lprintf("OUTPUT_DEBUG_STRING PID=%lu TID=%lu\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                );
            }

            // Find the process in the process list
            assert(nProcesses);
            i = 0;
            while (i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId) {
                ++i;
            }
            assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);
            pProcessInfo = &ProcessListInfo[i];

            assert(!DebugEvent.u.DebugString.fUnicode);

            LPSTR lpDebugStringData = readProcessString(pProcessInfo->hProcess,
                                                        DebugEvent.u.DebugString.lpDebugStringData,
                                                        DebugEvent.u.DebugString.nDebugStringLength);

            fputs(lpDebugStringData, stderr);

            free(lpDebugStringData);
            break;

        case RIP_EVENT:
            if (pOptions->debug_flag) {
                lprintf("RIP PID=%lu TID=%lu\r\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                );
            }
            break;

        default:
            if (pOptions->debug_flag) {
                lprintf("EVENT%lu PID=%lu TID=%lu\r\n",
                    DebugEvent.dwDebugEventCode,
                    DebugEvent.dwProcessId,
                    DebugEvent.dwThreadId
                );
            }
            break;
        }

        // Resume executing the thread that reported the debugging event.
        ContinueDebugEvent(
            DebugEvent.dwProcessId,
            DebugEvent.dwThreadId,
            dwContinueStatus
        );
    }

    return TRUE;
}
