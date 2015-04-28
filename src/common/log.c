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
#include <psapi.h>
#include <dbghelp.h>
#include <ntstatus.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "outdbg.h"
#include "paths.h"
#include "symbols.h"
#include "log.h"


static void
defaultCallback(const char *s)
{
    OutputDebugStringA(s);
}


static DumpCallback g_Cb = defaultCallback;


void
setDumpCallback(DumpCallback cb)
{
    g_Cb = cb;
}


#ifdef __GNUC__
    __attribute__ ((format (printf, 1, 2)))
#endif
int lprintf(const char * format, ...)
{
    char szBuffer[1024];
    int retValue;
    va_list ap;

    va_start(ap, format);
    retValue = _vsnprintf(szBuffer, sizeof szBuffer, format, ap);
    va_end(ap);

    g_Cb(szBuffer);

    return retValue;
}


static BOOL
dumpSourceCode(LPCTSTR lpFileName, DWORD dwLineNumber);


#define MAX_SYM_NAME_SIZE 512


static void
dumpContext(
#ifdef _WIN64
            PWOW64_CONTEXT pContext
#else
            PCONTEXT pContext
#endif
)
{
    // Show the registers
    lprintf(_T("Registers:\r\n"));
    if (pContext->ContextFlags & CONTEXT_INTEGER) {
        lprintf(
            _T("eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx\r\n"),
            pContext->Eax,
            pContext->Ebx,
            pContext->Ecx,
            pContext->Edx,
            pContext->Esi,
            pContext->Edi
        );
    }
    if (pContext->ContextFlags & CONTEXT_CONTROL) {
        lprintf(
            _T("eip=%08lx esp=%08lx ebp=%08lx iopl=%1lx %s %s %s %s %s %s %s %s %s %s\r\n"),
            pContext->Eip,
            pContext->Esp,
            pContext->Ebp,
            (pContext->EFlags >> 12) & 3,    //  IOPL level value
            pContext->EFlags & 0x00100000 ? "vip" : "   ",    //  VIP (virtual interrupt pending)
            pContext->EFlags & 0x00080000 ? "vif" : "   ",    //  VIF (virtual interrupt flag)
            pContext->EFlags & 0x00000800 ? "ov" : "nv",    //  VIF (virtual interrupt flag)
            pContext->EFlags & 0x00000400 ? "dn" : "up",    //  OF (overflow flag)
            pContext->EFlags & 0x00000200 ? "ei" : "di",    //  IF (interrupt enable flag)
            pContext->EFlags & 0x00000080 ? "ng" : "pl",    //  SF (sign flag)
            pContext->EFlags & 0x00000040 ? "zr" : "nz",    //  ZF (zero flag)
            pContext->EFlags & 0x00000010 ? "ac" : "na",    //  AF (aux carry flag)
            pContext->EFlags & 0x00000004 ? "po" : "pe",    //  PF (parity flag)
            pContext->EFlags & 0x00000001 ? "cy" : "nc"    //  CF (carry flag)
        );
    }
    if (pContext->ContextFlags & CONTEXT_SEGMENTS) {
        lprintf(
            _T("cs=%04lx  ss=%04lx  ds=%04lx  es=%04lx  fs=%04lx  gs=%04lx"),
            pContext->SegCs,
            pContext->SegSs,
            pContext->SegDs,
            pContext->SegEs,
            pContext->SegFs,
            pContext->SegGs
        );
        if (pContext->ContextFlags & CONTEXT_CONTROL) {
            lprintf(
                _T("             efl=%08lx"),
                pContext->EFlags
            );
        }
    }
    else {
        if (pContext->ContextFlags & CONTEXT_CONTROL) {
            lprintf(
                _T("                                                                       efl=%08lx"),
                pContext->EFlags
            );
        }
    }
    lprintf(_T("\r\n\r\n"));
}


void
dumpStack(HANDLE hProcess, HANDLE hThread,
          const CONTEXT *pTargetContext)
{
    DWORD MachineType;

    CONTEXT Context;
    ZeroMemory(&Context, sizeof Context);
    Context.ContextFlags = CONTEXT_FULL;
    PCONTEXT pContext;

    STACKFRAME64 StackFrame;
    ZeroMemory(&StackFrame, sizeof StackFrame);

    if (pTargetContext) {
        assert(hProcess == GetCurrentProcess());
    }

#ifdef _WIN64
    BOOL bWow64 = FALSE;
    WOW64_CONTEXT Wow64Context;
    IsWow64Process(hProcess, &bWow64);
    if (bWow64) {
        MachineType = IMAGE_FILE_MACHINE_I386;
        ZeroMemory(&Wow64Context, sizeof Wow64Context);
        Wow64Context.ContextFlags = WOW64_CONTEXT_FULL;
        if (!Wow64GetThreadContext(hThread, &Wow64Context)) {
            // XXX: This happens with WINE after EXIT_PROCESS_DEBUG_EVENT
            return;
        }
        assert(pTargetContext == NULL);
        pContext = (PCONTEXT)&Wow64Context;
        dumpContext(&Wow64Context);
        StackFrame.AddrPC.Offset = Wow64Context.Eip;
        StackFrame.AddrPC.Mode = AddrModeFlat;
        StackFrame.AddrStack.Offset = Wow64Context.Esp;
        StackFrame.AddrStack.Mode = AddrModeFlat;
        StackFrame.AddrFrame.Offset = Wow64Context.Ebp;
        StackFrame.AddrFrame.Mode = AddrModeFlat;
    } else {
#else
    {
#endif
        if (pTargetContext) {
            Context = *pTargetContext;
        } else {
            if (!GetThreadContext(hThread, &Context)) {
                // XXX: This happens with WINE after EXIT_PROCESS_DEBUG_EVENT
                return;
            }
        }
        pContext = &Context;

#ifndef _WIN64
        MachineType = IMAGE_FILE_MACHINE_I386;
        dumpContext(pContext);
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
    }

    if (MachineType == IMAGE_FILE_MACHINE_I386) {
        lprintf( _T("AddrPC   Params\r\n") );
    } else {
        lprintf( _T("AddrPC           Params\r\n") );
    }

    int nudge = 0;

    while (TRUE) {
        TCHAR szSymName[MAX_SYM_NAME_SIZE] = _T("");
        TCHAR szFileName[MAX_PATH] = _T("");
        DWORD dwLineNumber = 0;

        DWORD PrevFrameStackOffset = StackFrame.AddrStack.Offset;
        if (!StackWalk64(
                MachineType,
                hProcess,
                hThread,
                &StackFrame,
                pContext,
                NULL,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                NULL
            )
        )
            break;

        // Basic sanity check to make sure  the frame is OK.  Bail if not.
        if (StackFrame.AddrFrame.Offset == 0)
            break;

        if (MachineType == IMAGE_FILE_MACHINE_I386) {
            lprintf(
                _T("%08lX %08lX %08lX %08lX"),
                (DWORD)StackFrame.AddrPC.Offset,
                (DWORD)StackFrame.Params[0],
                (DWORD)StackFrame.Params[1],
                (DWORD)StackFrame.Params[2]
            );
        } else {
            lprintf(
                _T("%016I64X %016I64X %016I64X %016I64X"),
                StackFrame.AddrPC.Offset,
                StackFrame.Params[0],
                StackFrame.Params[1],
                StackFrame.Params[2]
            );
        }

        BOOL bSymbol = TRUE;
        BOOL bLine = FALSE;

        DWORD64 AddrPC = StackFrame.AddrPC.Offset;
        HMODULE hModule = (HMODULE)(INT_PTR)SymGetModuleBase64(hProcess, AddrPC);
        TCHAR szModule[MAX_PATH];
        if (hModule &&
            GetModuleFileNameEx(hProcess, hModule, szModule, MAX_PATH)) {

            lprintf( _T("  %s"), getBaseName(szModule));

            bSymbol = GetSymFromAddr(hProcess, AddrPC + nudge, szSymName, MAX_SYM_NAME_SIZE);
            if (bSymbol) {
                UnDecorateSymbolName(szSymName, szSymName, MAX_SYM_NAME_SIZE, UNDNAME_COMPLETE);

                lprintf( _T("!%s"), szSymName);

                bLine = GetLineFromAddr(hProcess, AddrPC + nudge, szFileName, MAX_PATH, &dwLineNumber);
                if (bLine) {
                    lprintf( _T("  [%s @ %ld]"), szFileName, dwLineNumber);
                }
            } else {
                lprintf( _T("!0x%I64x"), AddrPC - (DWORD)(INT_PTR)hModule);
            }
        }

        lprintf(_T("\r\n"));

        if (bLine) {
            dumpSourceCode(szFileName, dwLineNumber);
        }

        if (StackFrame.AddrStack.Offset < PrevFrameStackOffset) {
            break;
        }

        if (StackFrame.AddrPC.Offset == 0xBAADF00D) {
            break;
        }

        /*
         * When we walk into the callers, StackFrame.AddrPC.Offset will not
         * contain the calling function's address, but rather the return
         * address.  This could be the next statement, or sometimes (for
         * no-return functions) a completely different function, so nudge the
         * address by one byte to ensure we get the information about the
         * calling statment itself.
         */
        nudge = -1;
    }

    lprintf(_T("\r\n"));
}


/*
 * Get the message string for the exception code.
 *
 * Per https://support.microsoft.com/en-us/kb/259693 one could supposedly get
 * these from ntdll.dll via FormatMessage but the matter of fact is that the
 * FormatMessage is hopeless for that, as described in:
 * - http://www.microsoft.com/msj/0497/hood/hood0497.aspx
 * - http://stackoverflow.com/questions/321898/how-to-get-the-name-description-of-an-exception
 * - http://www.tech-archive.net/Archive/Development/microsoft.public.win32.programmer.kernel/2006-05/msg00683.html
 */
static LPCTSTR
getExceptionString(DWORD ExceptionCode)
{
    switch (ExceptionCode) {

    case EXCEPTION_GUARD_PAGE: // 0x80000001
        return _T("Guard Page Exception");
    case EXCEPTION_DATATYPE_MISALIGNMENT: // 0x80000002
        return _T("Alignment Fault");
    case EXCEPTION_BREAKPOINT: // 0x80000003
    case STATUS_WX86_BREAKPOINT: // 0x4000001F
        return _T("Breakpoint");
    case EXCEPTION_SINGLE_STEP: // 0x80000004
        return _T("Single Step");

    case EXCEPTION_ACCESS_VIOLATION: // 0xC0000005
        return _T("Access Violation");
    case EXCEPTION_IN_PAGE_ERROR: // 0xC0000006
        return _T("In Page Error");

    case EXCEPTION_INVALID_HANDLE: // 0xC0000008
        return _T("Invalid Handle");

    case EXCEPTION_ILLEGAL_INSTRUCTION: // 0xC000001D
        return _T("Illegal Instruction");

    case EXCEPTION_NONCONTINUABLE_EXCEPTION: // 0xC0000025
        return _T("Cannot Continue");
    case EXCEPTION_INVALID_DISPOSITION: // 0xC0000026
        return _T("Invalid Disposition");

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: // 0xC000008C
        return _T("Array bounds exceeded");
    case EXCEPTION_FLT_DENORMAL_OPERAND: // 0xC000008D
        return _T("Floating-point denormal operand");
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: // 0xC000008E
        return _T("Floating-point division by zero");
    case EXCEPTION_FLT_INEXACT_RESULT: // 0xC000008F
        return _T("Floating-point inexact result");
    case EXCEPTION_FLT_INVALID_OPERATION: // 0xC0000090
        return _T("Floating-point invalid operation");
    case EXCEPTION_FLT_OVERFLOW: // 0xC0000091
        return _T("Floating-point overflow");
    case EXCEPTION_FLT_STACK_CHECK: // 0xC0000092
        return _T("Floating-point stack check");
    case EXCEPTION_FLT_UNDERFLOW: // 0xC0000093
        return _T("Floating-point underflow");
    case EXCEPTION_INT_DIVIDE_BY_ZERO: // 0xC0000094
        return _T("Integer division by zero");
    case EXCEPTION_INT_OVERFLOW:  // 0xC0000095
        return _T("Integer overflow");
    case EXCEPTION_PRIV_INSTRUCTION: // 0xC0000096
        return _T("Privileged instruction");

    case EXCEPTION_STACK_OVERFLOW: // 0xC00000FD
        return _T("Stack Overflow");

    case EXCEPTION_POSSIBLE_DEADLOCK: // 0xC0000194
        return _T("Possible deadlock condition");

    case DBG_TERMINATE_THREAD: // 0x40010003
        return _T("Terminate Thread");
    case DBG_TERMINATE_PROCESS: // 0x40010004
        return _T("Terminate Process");
    case DBG_CONTROL_C: // 0x40010005
        return _T("Control+C");
    case DBG_CONTROL_BREAK: // 0x40010008
        return _T("Control+Break");

    case RPC_S_UNKNOWN_IF:
        return _T("Unknown Interface");
    case RPC_S_SERVER_UNAVAILABLE:
        return _T("Server Unavailable");

    default:
        return NULL;
    }
}


void
dumpException(HANDLE hProcess,
              PEXCEPTION_RECORD pExceptionRecord)
{
    DWORD ExceptionCode = pExceptionRecord->ExceptionCode;

    TCHAR szModule[MAX_PATH];
    LPCTSTR lpcszProcess;
    HMODULE hModule;

    if (GetModuleFileNameEx(hProcess, NULL, szModule, MAX_PATH)) {
        lpcszProcess = getBaseName(szModule);
    } else {
        lpcszProcess = _T("Application");
    }

    // First print information about the type of fault
    lprintf(_T("%s caused"), lpcszProcess);

    LPCTSTR lpcszException = getExceptionString(ExceptionCode);
    if (lpcszException) {
        LPCTSTR lpszArticle;
        switch (lpcszException[0]) {
        case 'A':
        case 'E':
        case 'I':
        case 'O':
        case 'U':
            lpszArticle = _T("an");
            break;
        default:
            lpszArticle = _T("a");
            break;
        }

        lprintf(_T(" %s %s"), lpszArticle, lpcszException);
    } else {
        lprintf(_T(" an Unknown [0x%lX] Exception"), ExceptionCode);
    }

    // Now print information about where the fault occured
    lprintf(_T(" at location %p"), pExceptionRecord->ExceptionAddress);
    if((hModule = (HMODULE)(INT_PTR)SymGetModuleBase64(hProcess, (DWORD64)(INT_PTR)pExceptionRecord->ExceptionAddress)) &&
       GetModuleFileNameEx(hProcess, hModule, szModule, sizeof szModule))
        lprintf(_T(" in module %s"), getBaseName(szModule));

    // If the exception was an access violation, print out some additional information, to the error log and the debugger.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363082%28v=vs.85%29.aspx
    if ((ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
         ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        pExceptionRecord->NumberParameters >= 2) {
        LPCTSTR lpszVerb;
        switch (pExceptionRecord->ExceptionInformation[0]) {
        case 0:
            lpszVerb = _T("Reading from");
            break;
        case 1:
            lpszVerb = _T("Writing to");
            break;
        case 8:
            lpszVerb = _T("DEP violation at");
            break;
        default:
            lpszVerb = _T("Accessing");
            break;
        }

        lprintf(" %s location %p", lpszVerb, (PVOID)pExceptionRecord->ExceptionInformation[1]);
    }

    lprintf(".\r\n\r\n");
}


static BOOL
dumpSourceCode(LPCTSTR lpFileName, DWORD dwLineNumber)
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

    i = 0;
    while(!feof(fp) && ++i <= (int) dwLineNumber + dwContext)
    {
        int c;

        if(i >= (int) dwLineNumber - dwContext)
        {
            lprintf(i == dwLineNumber ? ">%5i: " : "%6i: ", i);
            while(!feof(fp) && (c = fgetc(fp)) != '\n')
                if(isprint(c))
                    lprintf("%c", c);
            lprintf("\r\n");
        }
        else
            while(!feof(fp) && (c = fgetc(fp)) != '\n')
                ;
    }

    fclose(fp);
    return TRUE;
}
