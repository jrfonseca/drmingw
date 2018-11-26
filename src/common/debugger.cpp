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


#include <map>

#include <assert.h>
#include <stdlib.h>

#include <windows.h>
#include <ntstatus.h>
#include <psapi.h>

#include "debugger.h"
#include "log.h"
#include "outdbg.h"
#include "symbols.h"
#include "paths.h"


typedef struct {
    HANDLE hThread;
}
THREAD_INFO, * PTHREAD_INFO;

typedef std::map< DWORD, THREAD_INFO > THREAD_INFO_LIST;

typedef struct {
    HANDLE hProcess;
    THREAD_INFO_LIST Threads;
    BOOL fBreakpointSignalled;
    BOOL fWowBreakpointSignalled;
}
PROCESS_INFO, * PPROCESS_INFO;

typedef std::map< DWORD, PROCESS_INFO> PROCESS_INFO_LIST;
static PROCESS_INFO_LIST g_Processes;


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

            pbOldPriv = (PBYTE)LocalAlloc(LMEM_FIXED, cbNeeded);
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


static BOOL
symCallbackDeferedSymbol(const char *szVerb, ULONG64 CallbackData)
{
    PIMAGEHLP_DEFERRED_SYMBOL_LOAD64 pData = (PIMAGEHLP_DEFERRED_SYMBOL_LOAD64)(UINT_PTR)CallbackData;
    lprintf("%s deferred symbol load of: %s (hFile = %p)\n", szVerb, pData->FileName, pData->hFile);
    return FALSE;
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

    if (1) {
        if (ActionCode == CBA_DEFERRED_SYMBOL_LOAD_PARTIAL) {
            PIMAGEHLP_DEFERRED_SYMBOL_LOAD64 pData = (PIMAGEHLP_DEFERRED_SYMBOL_LOAD64)(UINT_PTR)CallbackData;
            lprintf("error: partial symbol load of %s\n", pData->FileName);
            return FALSE;
        }
    } else {
        // Debug partial symbols
        switch (ActionCode) {
        case CBA_DEFERRED_SYMBOL_LOAD_START:
            return symCallbackDeferedSymbol("Starting", CallbackData);
        case CBA_DEFERRED_SYMBOL_LOAD_COMPLETE:
            return symCallbackDeferedSymbol("Completed", CallbackData);
        case CBA_DEFERRED_SYMBOL_LOAD_FAILURE:
            return symCallbackDeferedSymbol("Failed", CallbackData);
        case CBA_DEFERRED_SYMBOL_LOAD_CANCEL:
            return FALSE;
        case CBA_DEFERRED_SYMBOL_LOAD_PARTIAL:
            return symCallbackDeferedSymbol("Partial", CallbackData);
        }
    }

    return FALSE;
}


// https://msdn.microsoft.com/en-us/library/aa366789.aspx
static BOOL
GetFileNameFromHandle(HANDLE hFile,
                      LPSTR lpszFilePath,
                      DWORD cchFilePath)
{
    static HMODULE hKernel32;
    typedef DWORD (WINAPI *PFNGETFINALPATHNAMEBYHANDLE)(HANDLE, LPSTR, DWORD, DWORD);
    static PFNGETFINALPATHNAMEBYHANDLE pfnGetFinalPathNameByHandle = NULL;
    if (!hKernel32) {
        hKernel32 = GetModuleHandleA("kernel32.dll");
        if (hKernel32) {
            pfnGetFinalPathNameByHandle = (PFNGETFINALPATHNAMEBYHANDLE)GetProcAddress(hKernel32, "GetFinalPathNameByHandleA");
        }
    }
    if (pfnGetFinalPathNameByHandle) {
        return pfnGetFinalPathNameByHandle(hFile, lpszFilePath, cchFilePath, 0) < cchFilePath;
    }

    DWORD dwFileSizeHi = 0;
    DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);
    if (dwFileSizeLo == 0 && dwFileSizeHi == 0) {
         // Cannot map a file with a length of zero.
         return FALSE;
    }

    BOOL bSuccess = FALSE;
    HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);
    if (hFileMap) {
        LPVOID pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);
        if (pMem) {
            if (GetMappedFileNameA(GetCurrentProcess(), pMem, lpszFilePath, cchFilePath)) {
                // Unlike the example, we don't bother translating the path with device name to drive letters.
                bSuccess = TRUE;
            }
            UnmapViewOfFile(pMem);
        }
        CloseHandle(hFileMap);
    }

    return bSuccess;
}


static DWORD
getModuleSize(HANDLE hProcess, LPVOID lpBaseOfDll)
{
    DWORD Size = 0;
    while (true) {
        LPCVOID lpAddress = (PBYTE)lpBaseOfDll + Size;

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(hProcess, lpAddress, &mbi, sizeof mbi) == 0 ||
            mbi.AllocationBase != lpBaseOfDll) {
            break;
        }

        Size += mbi.RegionSize;
    }

    return Size;
}


static void
loadModule(HANDLE hProcess, HANDLE hFile, PCSTR pszImageName, LPVOID lpBaseOfDll)
{
    bool deferred = SymGetOptions() & SYMOPT_DEFERRED_LOADS;

    // We must pass DllSize for deferred symbols to work correctly
    // https://groups.google.com/forum/#!topic/comp.os.ms-windows.programmer.win32/ulkwYhM3020
    DWORD DllSize = 0;
    if (deferred) {
        DllSize = getModuleSize(hProcess, lpBaseOfDll);
    }

    if (!SymLoadModuleEx(hProcess, hFile, pszImageName, NULL, (UINT_PTR)lpBaseOfDll, DllSize, NULL, 0)) {
        OutputDebug("warning: SymLoadModule64 failed: 0x%08lx\n", GetLastError());
    }

    // DbgHelp keeps an copy of hFile, and closing the handle here may cause
    // CBA_DEFERRED_SYMBOL_LOAD_PARTIAL events.
    if (hFile && !deferred) {
        CloseHandle(hFile);
    }
}


static char *
readProcessString(HANDLE hProcess, LPCVOID lpBaseAddress, SIZE_T nSize)
{
    LPSTR lpszBuffer = (LPSTR)malloc(nSize + 1);
    SIZE_T NumberOfBytesRead = 0;

    if (!ReadProcessMemory(hProcess, lpBaseAddress, lpszBuffer, nSize, &NumberOfBytesRead)) {
        lpszBuffer[0] = '\0';
    }

    assert(NumberOfBytesRead <= nSize);
    lpszBuffer[NumberOfBytesRead] = '\0';
    return lpszBuffer;
}


static BOOL
getThreadContext(HANDLE hProcess, HANDLE hThread,
                 PCONTEXT pContext)
{
    ZeroMemory(pContext, sizeof *pContext);

    BOOL bWow64 = FALSE;
    if (HAVE_WIN64) {
        IsWow64Process(hProcess, &bWow64);
    }

    BOOL bSuccess;
    if (bWow64) {
        PWOW64_CONTEXT pWow64Context = reinterpret_cast<PWOW64_CONTEXT>(pContext);
        static_assert(sizeof *pContext >= sizeof *pWow64Context, "WOW64_CONTEXT should fit in CONTEXT");
        pWow64Context->ContextFlags = WOW64_CONTEXT_FULL;
        bSuccess = Wow64GetThreadContext(hThread, pWow64Context);
    } else {
        pContext->ContextFlags = CONTEXT_FULL;
        bSuccess = GetThreadContext(hThread, pContext);
    }

    return bSuccess;
}


// Trap a particular thread
BOOL
TrapThread(DWORD dwProcessId, DWORD dwThreadId)
{
    PPROCESS_INFO pProcessInfo;
    PTHREAD_INFO pThreadInfo;
    HANDLE hProcess;
    HANDLE hThread;

    /* FIXME: synchronize access to globals */

    pProcessInfo = &g_Processes[dwProcessId];
    hProcess = pProcessInfo->hProcess;
    assert(hProcess);

    pThreadInfo = &pProcessInfo->Threads[dwThreadId];
    hThread = pThreadInfo->hThread;
    assert(hThread);

    DWORD dwRet = SuspendThread(hThread);
    if (dwRet != (DWORD)-1) {
        CONTEXT Context;
        if (getThreadContext(hProcess, hThread, &Context)) {
            dumpStack(hProcess, hThread, &Context);
        }

        // TODO: Flag fTerminating

        exit(3);
    }

    TerminateProcess(hProcess, 3);

    return TRUE;
}

// Abnormal termination can yield all sort of exit codes:
// - abort exits with 3
// - MS C/C++ Runtime might also exit with
//   - STATUS_INVALID_CRUNTIME_PARAMETER:
//   - STATUS_STACK_BUFFER_OVERRUN: // STATUS_SECURITY_CHECK_FAILURE
//   - STATUS_FATAL_APP_EXIT
// - EnterCriticalSection with unitalize CS will terminate with the calling HMODULE
//
static BOOL
isAbnormalExitCode(DWORD dwExitCode)
{
    return dwExitCode > 255 || dwExitCode == 3;
}


BOOL DebugMainLoop(const DebugOptions *pOptions)
{
    BOOL fFinished = FALSE;
    BOOL fTerminating = FALSE;

    while(!fFinished)
    {
        DEBUG_EVENT DebugEvent;            // debugging event information
        DWORD dwContinueStatus = DBG_CONTINUE;    // exception continuation
        PPROCESS_INFO pProcessInfo;
        PTHREAD_INFO pThreadInfo;
        HANDLE hProcess;

        // Wait for a debugging event to occur. The second parameter indicates
        // that the function does not return until a debugging event occurs.
        if(!WaitForDebugEvent(&DebugEvent, INFINITE))
        {
            OutputDebug("WaitForDebugEvent: 0x%08lx", GetLastError());

            return FALSE;
        }

        // Process the debugging event code.
        switch (DebugEvent.dwDebugEventCode) {
        case EXCEPTION_DEBUG_EVENT: {
            PEXCEPTION_RECORD pExceptionRecord = &DebugEvent.u.Exception.ExceptionRecord;
            NTSTATUS ExceptionCode = pExceptionRecord->ExceptionCode;

            // Process the exception code. When handling
            // exceptions, remember to set the continuation
            // status parameter (dwContinueStatus). This value
            // is used by the ContinueDebugEvent function.
            if (pOptions->verbose_flag) {
                lprintf("EXCEPTION PID=%lu TID=%lu ExceptionCode=0x%lx dwFirstChance=%lu\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        pExceptionRecord->ExceptionCode,
                        DebugEvent.u.Exception.dwFirstChance
                );
            }

            // Find the process in the process list
            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];

            dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;

            if (DebugEvent.u.Exception.dwFirstChance) {
                if (pExceptionRecord->ExceptionCode == (DWORD)STATUS_BREAKPOINT) {
                    // Signal the aedebug event
                    if (!pProcessInfo->fBreakpointSignalled) {
                        pProcessInfo->fBreakpointSignalled = TRUE;

                        if (pOptions->hEvent) {
                            SetEvent(pOptions->hEvent);
                            CloseHandle(pOptions->hEvent);
                        }

                        if (pOptions->dwThreadId) {
                            DWORD dwThreadId = pOptions->dwThreadId;
                            const DWORD dwFailed = (DWORD)-1;
                            DWORD dwRet = dwFailed;
                            pThreadInfo = &pProcessInfo->Threads[dwThreadId];
                            HANDLE hThread = pThreadInfo->hThread;
                            if (hThread != NULL) {
                                dwRet = ResumeThread(hThread);
                            }
                            if (dwRet == dwFailed) {
                                lprintf("error: failed to resume thread %lu\n", dwThreadId);
                            }
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

                if (ExceptionCode == STATUS_WX86_BREAKPOINT) {
                    if (!pProcessInfo->fWowBreakpointSignalled) {
                        pProcessInfo->fWowBreakpointSignalled = TRUE;
                        dwContinueStatus = DBG_CONTINUE;
                        break;
                    }
                }

               /*
                 * Ignore thread naming exception.
                 *
                 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
                 *
                 * TODO: Note down the thread name
                 */
                if (ExceptionCode == 0x406d1388) {
                    dwContinueStatus = DBG_CONTINUE;
                    break;
                }

                if (ExceptionCode == DBG_CONTROL_C ||
                    ExceptionCode == DBG_CONTROL_BREAK) {
                    dwContinueStatus = DBG_CONTINUE;
                } else if (!pOptions->first_chance) {
                    // Ignore other first change exceptions
                    break;
                }
            }

            dumpException(pProcessInfo->hProcess,
                          &DebugEvent.u.Exception.ExceptionRecord);

            // Find the thread in the thread list
            THREAD_INFO_LIST::const_iterator it;
            for (it = pProcessInfo->Threads.begin(); it != pProcessInfo->Threads.end(); ++it) {
                DWORD dwThreadId = it->first;
                HANDLE hThread = it->second.hThread;
                if (dwThreadId != DebugEvent.dwThreadId &&
                    ExceptionCode != STATUS_BREAKPOINT &&
                    ExceptionCode != STATUS_WX86_BREAKPOINT &&
                    ExceptionCode != DBG_CONTROL_C &&
                    ExceptionCode != DBG_CONTROL_BREAK) {
                    continue;
                }

                CONTEXT Context;
                if (!getThreadContext(hProcess, hThread, &Context)) {
                    continue;
                }

                dumpStack(pProcessInfo->hProcess, hThread, &Context);
            }

            if (!DebugEvent.u.Exception.dwFirstChance) {
                /*
                 * Terminate the process. As continuing would cause the JIT debugger
                 * to be invoked again.
                 */
                fTerminating = TRUE;
                TerminateProcess(pProcessInfo->hProcess, (UINT)ExceptionCode);
            }

            break;
        }

        case CREATE_THREAD_DEBUG_EVENT:
            if (pOptions->verbose_flag) {
                lprintf("CREATE_THREAD PID=%lu TID=%lu\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                );
            }

            // Add the thread to the thread list
            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];
            pThreadInfo = &pProcessInfo->Threads[DebugEvent.dwThreadId];
            pThreadInfo->hThread = DebugEvent.u.CreateThread.hThread;
            break;

        case CREATE_PROCESS_DEBUG_EVENT: {
            HANDLE hFile = DebugEvent.u.CreateProcessInfo.hFile;

            char szImageName[MAX_PATH];
            char *lpImageName = NULL;
            if (hFile && GetFileNameFromHandle(hFile, szImageName, _countof(szImageName))) {
                lpImageName = szImageName;
            }

            if (pOptions->verbose_flag) {
                PCSTR lpModuleName = lpImageName ? getBaseName(lpImageName) : "";

                lprintf("CREATE_PROCESS PID=%lu TID=%lu lpBaseOfImage=%p %s\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.CreateProcessInfo.lpBaseOfImage,
                        lpModuleName
                );
            }

            hProcess = DebugEvent.u.CreateProcessInfo.hProcess;

            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];
            pProcessInfo->hProcess = hProcess;

            pThreadInfo = &pProcessInfo->Threads[DebugEvent.dwThreadId];
            pThreadInfo->hThread = DebugEvent.u.CreateProcessInfo.hThread;

            if (!InitializeSym(hProcess, FALSE)) {
                OutputDebug("error: SymInitialize failed: 0x%08lx\n", GetLastError());
                exit(EXIT_FAILURE);
            }

            SymRegisterCallback64(hProcess, &symCallback, 0);

            loadModule(hProcess, hFile, lpImageName, DebugEvent.u.CreateProcessInfo.lpBaseOfImage);

            break;
        }

        case EXIT_THREAD_DEBUG_EVENT:
            if (pOptions->verbose_flag) {
                lprintf("EXIT_THREAD PID=%lu TID=%lu dwExitCode=0x%lx\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.ExitThread.dwExitCode
                );
            }

            // Remove the thread from the thread list
            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];
            hProcess = pProcessInfo->hProcess;

            // Dump the stack on abort()
            if (!fTerminating && isAbnormalExitCode(DebugEvent.u.ExitThread.dwExitCode)) {
                pThreadInfo = &pProcessInfo->Threads[DebugEvent.dwThreadId];
                HANDLE hThread = pThreadInfo->hThread;
                CONTEXT Context;
                if (getThreadContext(hProcess, hThread, &Context)) {
                    dumpStack(hProcess, hThread, &Context);
                }
            }

            pProcessInfo->Threads.erase(DebugEvent.dwThreadId);
            break;

        case EXIT_PROCESS_DEBUG_EVENT: {
            if (pOptions->verbose_flag) {
                lprintf("EXIT_PROCESS PID=%lu TID=%lu dwExitCode=0x%lx\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.ExitProcess.dwExitCode
                );
            }

            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];
            hProcess = pProcessInfo->hProcess;

            // Dump the stack on abort()
            if (!fTerminating && isAbnormalExitCode(DebugEvent.u.ExitThread.dwExitCode)) {
                pThreadInfo = &pProcessInfo->Threads[DebugEvent.dwThreadId];
                HANDLE hThread = pThreadInfo->hThread;
                CONTEXT Context;
                if (getThreadContext(hProcess, hThread, &Context)) {
                    dumpStack(hProcess, hThread, &Context);
                }
            }

            // Remove the process from the process list
            g_Processes.erase(DebugEvent.dwProcessId);

            if (!SymCleanup(hProcess)) {
                OutputDebug("SymCleanup failed with 0x%08lx\n", GetLastError());
            }

            if (g_Processes.empty()) {
                fFinished = TRUE;
            }

            break;
        }

        case LOAD_DLL_DEBUG_EVENT: {
            HANDLE hFile = DebugEvent.u.LoadDll.hFile;

            char szImageName[MAX_PATH];
            char *lpImageName = NULL;
            if (hFile && GetFileNameFromHandle(hFile, szImageName, _countof(szImageName))) {
                lpImageName = szImageName;
            }

            if (pOptions->verbose_flag) {
                PCSTR lpModuleName = lpImageName ? getBaseName(lpImageName) : "";

                lprintf("LOAD_DLL PID=%lu TID=%lu lpBaseOfDll=%p %s\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.LoadDll.lpBaseOfDll,
                        lpModuleName
                );
            }

            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];
            hProcess = pProcessInfo->hProcess;

            loadModule(hProcess, hFile, lpImageName, DebugEvent.u.LoadDll.lpBaseOfDll);

            break;
        }

        case UNLOAD_DLL_DEBUG_EVENT:
            if (pOptions->verbose_flag) {
                lprintf("UNLOAD_DLL PID=%lu TID=%lu lpBaseOfDll=%p\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId,
                        DebugEvent.u.UnloadDll.lpBaseOfDll
                );
            }

            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];
            hProcess = pProcessInfo->hProcess;

            SymUnloadModule64(hProcess, (UINT_PTR)DebugEvent.u.UnloadDll.lpBaseOfDll);

            break;

        case OUTPUT_DEBUG_STRING_EVENT: {
            if (pOptions->verbose_flag) {
                lprintf("OUTPUT_DEBUG_STRING PID=%lu TID=%lu\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                );
            }

            pProcessInfo = &g_Processes[DebugEvent.dwProcessId];

            assert(!DebugEvent.u.DebugString.fUnicode);

            LPSTR lpDebugStringData = readProcessString(pProcessInfo->hProcess,
                                                        DebugEvent.u.DebugString.lpDebugStringData,
                                                        DebugEvent.u.DebugString.nDebugStringLength);

            lprintf("%s", lpDebugStringData);

            free(lpDebugStringData);
            break;
        }

        case RIP_EVENT:
            if (pOptions->verbose_flag) {
                lprintf("RIP PID=%lu TID=%lu\n",
                        DebugEvent.dwProcessId,
                        DebugEvent.dwThreadId
                );
            }
            break;

        default:
            if (pOptions->verbose_flag) {
                lprintf("EVENT%lu PID=%lu TID=%lu\n",
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
