/* log.c
 *
 *
 * Jose Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "debugger.h"
#include "dialog.h"
#include "misc.h"
#include "pehelp.h"
#include "mgwhelp.h"
#include "symbols.h"
#include "log.h"

static TCHAR *lpszLog = NULL;

//int __cdecl lprintf(const TCHAR * format, ...) __attribute__ ((format (printf, 1, 2)))

void lflush(void)
{
    UpdateText(lpszLog);
}

int __cdecl lprintf(const TCHAR * format, ...)
{
    TCHAR szBuffer[1024];
    int retValue;
    va_list ap;

    va_start(ap, format);
    retValue = _vstprintf(szBuffer, format, ap);
    va_end(ap);

    if(!lpszLog)
    {
        lpszLog = HeapAlloc(
            GetProcessHeap(),
            0,
            (retValue + 1)*sizeof(TCHAR)
        );

        lstrcpy(lpszLog, szBuffer);
    }
    else
    {
        lpszLog = HeapReAlloc(
            GetProcessHeap(),
            0,
            lpszLog,
            (lstrlen(lpszLog) + retValue + 1)*sizeof(TCHAR)
        );

        lstrcat(lpszLog, szBuffer);
    }

    if(strchr(szBuffer, '\n'))
        lflush();
    return retValue;
}

//#define lprintf OutputDebug


LPTSTR GetBaseName(LPTSTR lpFileName)
{
    LPTSTR lpChar = lpFileName + lstrlen(lpFileName);

    while(lpChar > lpFileName && *(lpChar - 1) != '\\' && *(lpChar - 1) != '/' && *(lpChar - 1) != ':')
        --lpChar;

    return lpChar;
}


#define MAX_SYM_NAME_SIZE 512

static BOOL
StackBackTrace(HANDLE hProcess, HANDLE hThread, PCONTEXT pContext)
{
    int i;
    PPROCESS_LIST_INFO pProcessListInfo;

    DWORD MachineType;
    STACKFRAME64 StackFrame;

    HMODULE hModule = NULL;
    TCHAR szModule[MAX_PATH];

    // Remove the process from the process list
    assert(nProcesses);
    i = 0;
    while(hProcess != ProcessListInfo[i].hProcess)
    {
        ++i;
        assert(i < nProcesses);
    }
    pProcessListInfo = &ProcessListInfo[i];

    assert(!bSymInitialized);

    SymSetOptions(/* SYMOPT_UNDNAME | */ SYMOPT_LOAD_LINES);
    if(SymInitialize(hProcess, NULL, TRUE))
        bSymInitialized = TRUE;
    else
        if(verbose_flag)
            lprintf(_T("SymInitialize: %s\r\n"), LastErrorMessage());

    memset( &StackFrame, 0, sizeof(StackFrame) );

    // Initialize the STACKFRAME structure for the first call.  This is only
    // necessary for Intel CPUs, and isn't mentioned in the documentation.
#if defined(_M_IX86)
    MachineType = IMAGE_FILE_MACHINE_I386;
    StackFrame.AddrPC.Offset = pContext->Eip;
    StackFrame.AddrPC.Mode = AddrModeFlat;
    StackFrame.AddrStack.Offset = pContext->Esp;
    StackFrame.AddrStack.Mode = AddrModeFlat;
    StackFrame.AddrFrame.Offset = pContext->Ebp;
    StackFrame.AddrFrame.Mode = AddrModeFlat;
#else
    MachineType = IMAGE_FILE_MACHINE_AMD64;
    StackFrame.AddrPC.Offset = pContext->Rip;
    StackFrame.AddrPC.Mode = AddrModeFlat;
    StackFrame.AddrStack.Offset = pContext->Rsp;
    StackFrame.AddrStack.Mode = AddrModeFlat;
    StackFrame.AddrFrame.Offset = pContext->Rbp;
    StackFrame.AddrFrame.Mode = AddrModeFlat;
#endif

    lprintf( _T("Call stack:\r\n") );

    if(verbose_flag)
        lprintf( _T("AddrPC     AddrReturn AddrFrame  AddrStack  Params\r\n") );

    while ( 1 )
        {
        BOOL bSuccess = FALSE;
        TCHAR szSymName[MAX_SYM_NAME_SIZE] = _T("");
        TCHAR szFileName[MAX_PATH] = _T("");
        DWORD dwLineNumber = 0;

        if(!StackWalk64(
                MachineType,
                hProcess,
                hThread,
                &StackFrame,
                pContext,
                NULL,
                SymFunctionTableAccess64,
                GetModuleBase64,
                NULL
            )
        )
            break;

        // Basic sanity check to make sure  the frame is OK.  Bail if not.
        if ( 0 == StackFrame.AddrFrame.Offset )
            break;

        if(verbose_flag)
            lprintf(
                _T("%08lX   %08lX   %08lX   %08lX   %08lX   %08lX   %08lX   %08lX\r\n"),
                StackFrame.AddrPC.Offset,
                StackFrame.AddrReturn.Offset,
                StackFrame.AddrFrame.Offset,
                StackFrame.AddrStack.Offset,
                StackFrame.Params[0],
                StackFrame.Params[1],
                StackFrame.Params[2],
                StackFrame.Params[3]
            );

        lprintf( _T("%08lX"), StackFrame.AddrPC.Offset);

        if((hModule = (HMODULE)(INT_PTR)GetModuleBase64(hProcess, StackFrame.AddrPC.Offset)) &&
           GetModuleFileNameEx(hProcess, hModule, szModule, sizeof(szModule)))
        {

            lprintf( _T("  %s:%08lX"), GetBaseName(szModule), StackFrame.AddrPC.Offset);

            // Find the module from the module list
            assert(nModules);
            i = 0;
            while(i < nModules && (pProcessListInfo->dwProcessId > ModuleListInfo[i].dwProcessId || (pProcessListInfo->dwProcessId == ModuleListInfo[i].dwProcessId && (LPVOID)hModule > ModuleListInfo[i].lpBaseAddress)))
                ++i;
            assert(ModuleListInfo[i].dwProcessId == pProcessListInfo->dwProcessId);
            assert(ModuleListInfo[i].lpBaseAddress == (LPVOID)hModule);
            assert(ModuleListInfo[i].dwProcessId == pProcessListInfo->dwProcessId && ModuleListInfo[i].lpBaseAddress == (LPVOID)hModule);

            if(bSymInitialized)
                if((bSuccess = GetSymFromAddr(hProcess, StackFrame.AddrPC.Offset, szSymName, MAX_SYM_NAME_SIZE)))
                {
                    UnDecorateSymbolName(szSymName, szSymName, MAX_SYM_NAME_SIZE, UNDNAME_COMPLETE);

                    lprintf( _T("  %s"), szSymName);

                    if(GetLineFromAddr(hProcess, StackFrame.AddrPC.Offset, szFileName, MAX_PATH, &dwLineNumber))
                        lprintf( _T("  %s:%ld"), GetBaseName(szFileName), dwLineNumber);
                }

            if(!bSuccess && (bSuccess = PEGetSymFromAddr(hProcess, StackFrame.AddrPC.Offset, szSymName, MAX_SYM_NAME_SIZE)))
                lprintf( _T("  %s"), szSymName);
        }

        lprintf(_T("\r\n"));

        if(bSuccess && DumpSource(szFileName, dwLineNumber))
            lprintf(_T("\r\n"));
    }

    if(bSymInitialized)
    {
        if(!SymCleanup(hProcess))
            assert(0);

        bSymInitialized = FALSE;
    }

    return TRUE;
}


BOOL LogException(DEBUG_EVENT DebugEvent)
{
    PPROCESS_LIST_INFO pProcessInfo;
    PTHREAD_LIST_INFO pThreadInfo;
    TCHAR szModule[MAX_PATH];
    HMODULE hModule;

    unsigned i;

    OutputDebug("%s\n", __FUNCTION__);

    assert(DebugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT);

    // Find the process in the process list
    assert(nProcesses);
    i = 0;
    while(i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId)
        ++i;
    assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);
    pProcessInfo = &ProcessListInfo[i];

    /*assert(!bSymInitialize);
    SymSetOptions(SYMOPT_LOAD_LINES);
    if(!pfnSymInitialize(hProcess, NULL, TRUE))
    {
        if(verbose_flag)
            ErrorMessageBox(_T("SymInitialize: %s"), LastErrorMessage());
        return FALSE;
    }
    bSymInitialized = TRUE;*/

    // First print information about the type of fault
    lprintf(_T("%s caused "),  GetModuleFileNameEx(pProcessInfo->hProcess, NULL, szModule, MAX_PATH) ? GetBaseName(szModule) : "Application");
    switch(DebugEvent.u.Exception.ExceptionRecord.ExceptionCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            lprintf(_T("an Access Violation"));
            break;

        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            lprintf(_T("an Array Bound Exceeded"));
            break;

        case EXCEPTION_BREAKPOINT:
            lprintf(_T("a Breakpoint"));
            break;

        case EXCEPTION_DATATYPE_MISALIGNMENT:
            lprintf(_T("a Datatype Misalignment"));
            break;

        case EXCEPTION_FLT_DENORMAL_OPERAND:
            lprintf(_T("a Float Denormal Operand"));
            break;

        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            lprintf(_T("a Float Divide By Zero"));
            break;

        case EXCEPTION_FLT_INEXACT_RESULT:
            lprintf(_T("a Float Inexact Result"));
            break;

        case EXCEPTION_FLT_INVALID_OPERATION:
            lprintf(_T("a Float Invalid Operation"));
            break;

        case EXCEPTION_FLT_OVERFLOW:
            lprintf(_T("a Float Overflow"));
            break;

        case EXCEPTION_FLT_STACK_CHECK:
            lprintf(_T("a Float Stack Check"));
            break;

        case EXCEPTION_FLT_UNDERFLOW:
            lprintf(_T("a Float Underflow"));
            break;

        case EXCEPTION_GUARD_PAGE:
            lprintf(_T("a Guard Page"));
            break;

        case EXCEPTION_ILLEGAL_INSTRUCTION:
            lprintf(_T("an Illegal Instruction"));
            break;

        case EXCEPTION_IN_PAGE_ERROR:
            lprintf(_T("an In Page Error"));
            break;

        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            lprintf(_T("an Integer Divide By Zero"));
            break;

        case EXCEPTION_INT_OVERFLOW:
            lprintf(_T("an Integer Overflow"));
            break;

        case EXCEPTION_INVALID_DISPOSITION:
            lprintf(_T("an Invalid Disposition"));
            break;

        case EXCEPTION_INVALID_HANDLE:
            lprintf(_T("an Invalid Handle"));
            break;

        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            lprintf(_T("a Noncontinuable Exception"));
            break;

        case EXCEPTION_PRIV_INSTRUCTION:
            lprintf(_T("a Privileged Instruction"));
            break;

        case EXCEPTION_SINGLE_STEP:
            lprintf(_T("a Single Step"));
            break;

        case EXCEPTION_STACK_OVERFLOW:
            lprintf(_T("a Stack Overflow"));
            break;

        case DBG_CONTROL_C:
            lprintf(_T("a Control+C"));
            break;

        case DBG_CONTROL_BREAK:
            lprintf(_T("a Control+Break"));
            break;

        case DBG_TERMINATE_THREAD:
            lprintf(_T("a Terminate Thread"));
            break;

        case DBG_TERMINATE_PROCESS:
            lprintf(_T("a Terminate Process"));
            break;

        case RPC_S_UNKNOWN_IF:
            lprintf(_T("an Unknown Interface"));
            break;

        case RPC_S_SERVER_UNAVAILABLE:
            lprintf(_T("a Server Unavailable"));
            break;

        default:
            /*
            static TCHAR szBuffer[512] = { 0 };

            // If not one of the "known" exceptions, try to get the string
            // from NTDLL.DLL's message table.

            FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                            GetModuleHandle(_T("NTDLL.DLL")),
                            dwCode, 0, szBuffer, sizeof(szBuffer), 0);
            */

            lprintf(_T("an Unknown [0x%lX] Exception"), DebugEvent.u.Exception.ExceptionRecord.ExceptionCode);
            break;
    }

    // Now print information about where the fault occured
    lprintf(_T(" at location %p"), DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress);
    if((hModule = (HMODULE)(INT_PTR)GetModuleBase64(pProcessInfo->hProcess, (DWORD64)(INT_PTR)DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress)) &&
       GetModuleFileNameEx(pProcessInfo->hProcess, hModule, szModule, sizeof szModule))
        lprintf(_T(" in module %s"), GetBaseName(szModule));

    // If the exception was an access violation, print out some additional information, to the error log and the debugger.
    if(DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
       DebugEvent.u.Exception.ExceptionRecord.NumberParameters >= 2)
        lprintf(" %s location %08lx",
            DebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[0] ? "Writing to" : "Reading from",
            DebugEvent.u.Exception.ExceptionRecord.ExceptionInformation[1]);

    lprintf(".\r\n\r\n");

    // Find the thread in the thread list
    assert(nThreads);
    for (i = 0; i < nThreads; ++i) {
        assert(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId);
        if (ThreadListInfo[i].dwThreadId == DebugEvent.dwThreadId) {
           lprintf(_T("XXXXXXXXXXXXXXXXXXXXXX\r\n"));
        }
        pThreadInfo = &ThreadListInfo[i];

        // Get the thread context
        CONTEXT Context;
        ZeroMemory(&Context, sizeof Context);
        Context.ContextFlags = CONTEXT_FULL;
        if(!GetThreadContext(pThreadInfo->hThread, &Context))
            assert(0);

        #ifdef _M_IX86    // Intel Only!

        // Show the registers
        lprintf(_T("Registers:\r\n"));
        if(Context.ContextFlags & CONTEXT_INTEGER)
            lprintf(
                _T("eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx\r\n"),
                Context.Eax,
                Context.Ebx,
                Context.Ecx,
                Context.Edx,
                Context.Esi,
                Context.Edi
            );
        if(Context.ContextFlags & CONTEXT_CONTROL)
            lprintf(
                _T("eip=%08lx esp=%08lx ebp=%08lx iopl=%1lx %s %s %s %s %s %s %s %s %s %s\r\n"),
                Context.Eip,
                Context.Esp,
                Context.Ebp,
                (Context.EFlags >> 12) & 3,    //  IOPL level value
                Context.EFlags & 0x00100000 ? "vip" : "   ",    //  VIP (virtual interrupt pending)
                Context.EFlags & 0x00080000 ? "vif" : "   ",    //  VIF (virtual interrupt flag)
                Context.EFlags & 0x00000800 ? "ov" : "nv",    //  VIF (virtual interrupt flag)
                Context.EFlags & 0x00000400 ? "dn" : "up",    //  OF (overflow flag)
                Context.EFlags & 0x00000200 ? "ei" : "di",    //  IF (interrupt enable flag)
                Context.EFlags & 0x00000080 ? "ng" : "pl",    //  SF (sign flag)
                Context.EFlags & 0x00000040 ? "zr" : "nz",    //  ZF (zero flag)
                Context.EFlags & 0x00000010 ? "ac" : "na",    //  AF (aux carry flag)
                Context.EFlags & 0x00000004 ? "po" : "pe",    //  PF (parity flag)
                Context.EFlags & 0x00000001 ? "cy" : "nc"    //  CF (carry flag)
            );
        if(Context.ContextFlags & CONTEXT_SEGMENTS)
        {
            lprintf(
                _T("cs=%04lx  ss=%04lx  ds=%04lx  es=%04lx  fs=%04lx  gs=%04lx"),
                Context.SegCs,
                Context.SegSs,
                Context.SegDs,
                Context.SegEs,
                Context.SegFs,
                Context.SegGs
            );
            if(Context.ContextFlags & CONTEXT_CONTROL)
                lprintf(
                    _T("             efl=%08lx"),
                    Context.EFlags
                );
        }
        else
            if(Context.ContextFlags & CONTEXT_CONTROL)
                lprintf(
                    _T("                                                                       efl=%08lx"),
                    Context.EFlags
                );
        lprintf(_T("\r\n\r\n"));

        #endif

        StackBackTrace(pProcessInfo->hProcess, pThreadInfo->hThread, &Context);

        /*if(bSymInitialized)
        {
            if(!pfnSymCleanup(hProcess))
                assert(0);
        }*/
    }

    lflush();
    return TRUE;
}

BOOL DumpSource(LPCTSTR lpFileName, DWORD dwLineNumber)
{
    FILE *fp;
    int i;
    TCHAR szFileName[MAX_PATH] = _T("");
    DWORD dwContext = 2;

    if(lpFileName[0] == '/' && lpFileName[1] == '/')
    {
        szFileName[0] = lpFileName[2];
        szFileName[1] = ':';
        lstrcpy(szFileName + 2, lpFileName + 3);
    }
    else
        lstrcpy(szFileName, lpFileName);

    if((fp = fopen(szFileName, "r")) == NULL)
        return FALSE;

    lprintf("\t...\r\n");

    i = 0;
    while(!feof(fp) && ++i <= (int) dwLineNumber + dwContext)
    {
        int c;

        if(i >= (int) dwLineNumber - dwContext)
        {
            lprintf(i == dwLineNumber ? ">\t" : "\t");
            while(!feof(fp) && (c = fgetc(fp)) != '\n')
                if(isprint(c))
                    lprintf("%c", c);
            lprintf("\r\n");
        }
        else
            while(!feof(fp) && (c = fgetc(fp)) != '\n')
                ;
    }

    lprintf("\t...\r\n");

    fclose(fp);
    return TRUE;
}
