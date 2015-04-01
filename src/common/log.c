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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "outdbg.h"
#include "paths.h"
#include "symbols.h"
#include "log.h"


static
#ifdef __GNUC__
    __attribute__ ((format (printf, 2, 3)))
#endif
int lprintf(DumpCallback cb, const char * format, ...)
{
    char szBuffer[1024];
    int retValue;
    va_list ap;

    va_start(ap, format);
    retValue = _vsnprintf(szBuffer, sizeof szBuffer, format, ap);
    va_end(ap);

    cb(szBuffer);

    return retValue;
}


static BOOL
dumpSourceCode(DumpCallback cb, LPCTSTR lpFileName, DWORD dwLineNumber);


#define MAX_SYM_NAME_SIZE 512


static void
dumpContext(DumpCallback cb,
#ifdef _WIN64
            PWOW64_CONTEXT pContext
#else
            PCONTEXT pContext
#endif
)
{
    // Show the registers
    lprintf(cb, _T("Registers:\r\n"));
    if (pContext->ContextFlags & CONTEXT_INTEGER) {
        lprintf(cb,
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
        lprintf(cb,
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
        lprintf(cb,
            _T("cs=%04lx  ss=%04lx  ds=%04lx  es=%04lx  fs=%04lx  gs=%04lx"),
            pContext->SegCs,
            pContext->SegSs,
            pContext->SegDs,
            pContext->SegEs,
            pContext->SegFs,
            pContext->SegGs
        );
        if (pContext->ContextFlags & CONTEXT_CONTROL) {
            lprintf(cb,
                _T("             efl=%08lx"),
                pContext->EFlags
            );
        }
    }
    else {
        if (pContext->ContextFlags & CONTEXT_CONTROL) {
            lprintf(cb,
                _T("                                                                       efl=%08lx"),
                pContext->EFlags
            );
        }
    }
    lprintf(cb, _T("\r\n\r\n"));
}


void
dumpStack(DumpCallback cb,
          HANDLE hProcess, HANDLE hThread,
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
        Wow64GetThreadContext(hThread, &Wow64Context);
        assert(pTargetContext == NULL);
        pContext = (PCONTEXT)&Wow64Context;
        dumpContext(cb, &Wow64Context);
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
            GetThreadContext(hThread, &Context);
        }
        pContext = &Context;

#ifndef _WIN64
        MachineType = IMAGE_FILE_MACHINE_I386;
        dumpContext(cb, pContext);
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
        lprintf(cb,  _T("AddrPC   Params\r\n") );
    } else {
        lprintf(cb,  _T("AddrPC           Params\r\n") );
    }

    while (TRUE) {
        TCHAR szSymName[MAX_SYM_NAME_SIZE] = _T("");
        TCHAR szFileName[MAX_PATH] = _T("");
        DWORD dwLineNumber = 0;

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
        if ( 0 == StackFrame.AddrFrame.Offset )
            break;

        if (MachineType == IMAGE_FILE_MACHINE_I386) {
            lprintf(cb,
                _T("%08lX %08lX %08lX %08lX"),
                (DWORD)StackFrame.AddrPC.Offset,
                (DWORD)StackFrame.Params[0],
                (DWORD)StackFrame.Params[1],
                (DWORD)StackFrame.Params[2]
            );
        } else {
            lprintf(cb,
                _T("%016I64X %016I64X %016I64X %016I64X"),
                StackFrame.AddrPC.Offset,
                StackFrame.Params[0],
                StackFrame.Params[1],
                StackFrame.Params[2]
            );
        }

        BOOL bSymbol = TRUE;
        BOOL bLine = FALSE;

        HMODULE hModule = (HMODULE)(INT_PTR)SymGetModuleBase64(hProcess, StackFrame.AddrPC.Offset);
        TCHAR szModule[MAX_PATH];
        if (hModule &&
            GetModuleFileNameEx(hProcess, hModule, szModule, MAX_PATH)) {

            lprintf(cb,  _T("  %s"), getBaseName(szModule));

            bSymbol = GetSymFromAddr(hProcess, StackFrame.AddrPC.Offset, szSymName, MAX_SYM_NAME_SIZE);
            if (bSymbol) {
                UnDecorateSymbolName(szSymName, szSymName, MAX_SYM_NAME_SIZE, UNDNAME_COMPLETE);

                lprintf(cb,  _T("!%s"), szSymName);

                bLine = GetLineFromAddr(hProcess, StackFrame.AddrPC.Offset, szFileName, MAX_PATH, &dwLineNumber);
                if (bLine) {
                    lprintf(cb,  _T("  [%s @ %ld]"), szFileName, dwLineNumber);
                }
            } else {
                lprintf(cb,  _T("!0x%I64x"), StackFrame.AddrPC.Offset - (DWORD)(INT_PTR)hModule);
            }
        }

        lprintf(cb, _T("\r\n"));

        if (bLine && dumpSourceCode(cb, szFileName, dwLineNumber))
            lprintf(cb, _T("\r\n"));
    }

    lprintf(cb, _T("\r\n"));
}


void
dumpException(DumpCallback cb,
              HANDLE hProcess,
              PEXCEPTION_RECORD pExceptionRecord)
{
    TCHAR szModule[MAX_PATH];
    HMODULE hModule;

    // First print information about the type of fault
    lprintf(cb, _T("%s caused "),  GetModuleFileNameEx(hProcess, NULL, szModule, MAX_PATH) ? getBaseName(szModule) : "Application");
    switch(pExceptionRecord->ExceptionCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            lprintf(cb, _T("an Access Violation"));
            break;

        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            lprintf(cb, _T("an Array Bound Exceeded"));
            break;

        case EXCEPTION_BREAKPOINT:
            lprintf(cb, _T("a Breakpoint"));
            break;

        case EXCEPTION_DATATYPE_MISALIGNMENT:
            lprintf(cb, _T("a Datatype Misalignment"));
            break;

        case EXCEPTION_FLT_DENORMAL_OPERAND:
            lprintf(cb, _T("a Float Denormal Operand"));
            break;

        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            lprintf(cb, _T("a Float Divide By Zero"));
            break;

        case EXCEPTION_FLT_INEXACT_RESULT:
            lprintf(cb, _T("a Float Inexact Result"));
            break;

        case EXCEPTION_FLT_INVALID_OPERATION:
            lprintf(cb, _T("a Float Invalid Operation"));
            break;

        case EXCEPTION_FLT_OVERFLOW:
            lprintf(cb, _T("a Float Overflow"));
            break;

        case EXCEPTION_FLT_STACK_CHECK:
            lprintf(cb, _T("a Float Stack Check"));
            break;

        case EXCEPTION_FLT_UNDERFLOW:
            lprintf(cb, _T("a Float Underflow"));
            break;

        case EXCEPTION_GUARD_PAGE:
            lprintf(cb, _T("a Guard Page"));
            break;

        case EXCEPTION_ILLEGAL_INSTRUCTION:
            lprintf(cb, _T("an Illegal Instruction"));
            break;

        case EXCEPTION_IN_PAGE_ERROR:
            lprintf(cb, _T("an In Page Error"));
            break;

        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            lprintf(cb, _T("an Integer Divide By Zero"));
            break;

        case EXCEPTION_INT_OVERFLOW:
            lprintf(cb, _T("an Integer Overflow"));
            break;

        case EXCEPTION_INVALID_DISPOSITION:
            lprintf(cb, _T("an Invalid Disposition"));
            break;

        case EXCEPTION_INVALID_HANDLE:
            lprintf(cb, _T("an Invalid Handle"));
            break;

        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            lprintf(cb, _T("a Noncontinuable Exception"));
            break;

        case EXCEPTION_PRIV_INSTRUCTION:
            lprintf(cb, _T("a Privileged Instruction"));
            break;

        case EXCEPTION_SINGLE_STEP:
            lprintf(cb, _T("a Single Step"));
            break;

        case EXCEPTION_STACK_OVERFLOW:
            lprintf(cb, _T("a Stack Overflow"));
            break;

        case DBG_CONTROL_C:
            lprintf(cb, _T("a Control+C"));
            break;

        case DBG_CONTROL_BREAK:
            lprintf(cb, _T("a Control+Break"));
            break;

        case DBG_TERMINATE_THREAD:
            lprintf(cb, _T("a Terminate Thread"));
            break;

        case DBG_TERMINATE_PROCESS:
            lprintf(cb, _T("a Terminate Process"));
            break;

        case RPC_S_UNKNOWN_IF:
            lprintf(cb, _T("an Unknown Interface"));
            break;

        case RPC_S_SERVER_UNAVAILABLE:
            lprintf(cb, _T("a Server Unavailable"));
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

            lprintf(cb, _T("an Unknown [0x%lX] Exception"), pExceptionRecord->ExceptionCode);
            break;
    }

    // Now print information about where the fault occured
    lprintf(cb, _T(" at location %p"), pExceptionRecord->ExceptionAddress);
    if((hModule = (HMODULE)(INT_PTR)SymGetModuleBase64(hProcess, (DWORD64)(INT_PTR)pExceptionRecord->ExceptionAddress)) &&
       GetModuleFileNameEx(hProcess, hModule, szModule, sizeof szModule))
        lprintf(cb, _T(" in module %s"), getBaseName(szModule));

    // If the exception was an access violation, print out some additional information, to the error log and the debugger.
    if(pExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
       pExceptionRecord->NumberParameters >= 2)
        lprintf(cb, " %s location %p",
            pExceptionRecord->ExceptionInformation[0] ? "Writing to" : "Reading from",
            (void *)pExceptionRecord->ExceptionInformation[1]);

    lprintf(cb, ".\r\n\r\n");
}


static BOOL
dumpSourceCode(DumpCallback cb, LPCTSTR lpFileName, DWORD dwLineNumber)
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

    lprintf(cb, "\t...\r\n");

    i = 0;
    while(!feof(fp) && ++i <= (int) dwLineNumber + dwContext)
    {
        int c;

        if(i >= (int) dwLineNumber - dwContext)
        {
            lprintf(cb, i == dwLineNumber ? ">\t" : "\t");
            while(!feof(fp) && (c = fgetc(fp)) != '\n')
                if(isprint(c))
                    lprintf(cb, "%c", c);
            lprintf(cb, "\r\n");
        }
        else
            while(!feof(fp) && (c = fgetc(fp)) != '\n')
                ;
    }

    lprintf(cb, "\t...\r\n");

    fclose(fp);
    return TRUE;
}
