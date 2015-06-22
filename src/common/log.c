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
#include <psapi.h>
#include <dbghelp.h>
#include <ntstatus.h>
#include <tlhelp32.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "outdbg.h"
#include "paths.h"
#include "symbols.h"
#include "log.h"


#ifndef STATUS_CPP_EH_EXCEPTION
#define STATUS_CPP_EH_EXCEPTION 0xE06D7363
#endif
#ifndef STATUS_CLR_EXCEPTION
#define STATUS_CLR_EXCEPTION 0xE0434f4D
#endif


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
dumpSourceCode(LPCSTR lpFileName, DWORD dwLineNumber);


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
    lprintf("Registers:\r\n");
    if (pContext->ContextFlags & CONTEXT_INTEGER) {
        lprintf(
            "eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx\r\n",
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
            "eip=%08lx esp=%08lx ebp=%08lx iopl=%1lx %s %s %s %s %s %s %s %s %s %s\r\n",
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
            "cs=%04lx  ss=%04lx  ds=%04lx  es=%04lx  fs=%04lx  gs=%04lx",
            pContext->SegCs,
            pContext->SegSs,
            pContext->SegDs,
            pContext->SegEs,
            pContext->SegFs,
            pContext->SegGs
        );
        if (pContext->ContextFlags & CONTEXT_CONTROL) {
            lprintf(
                "             efl=%08lx",
                pContext->EFlags
            );
        }
    }
    else {
        if (pContext->ContextFlags & CONTEXT_CONTROL) {
            lprintf(
                "                                                                       efl=%08lx",
                pContext->EFlags
            );
        }
    }
    lprintf("\r\n\r\n");
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
        assert((pTargetContext->ContextFlags & CONTEXT_FULL) == CONTEXT_FULL);
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
        lprintf( "AddrPC   Params\r\n" );
    } else {
        lprintf( "AddrPC           Params\r\n" );
    }

    int nudge = 0;

    while (TRUE) {
        char szSymName[MAX_SYM_NAME_SIZE] = "";
        char szFileName[MAX_PATH] = "";
        DWORD dwLineNumber = 0;

        DWORD PrevFrameStackOffset = StackFrame.AddrStack.Offset;
        if (!StackWalk64(
                MachineType,
                hProcess,
                hThread,
                &StackFrame,
                pContext,
                NULL, // ReadMemoryRoutine
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                NULL // TranslateAddress
            )
        )
            break;

        // Basic sanity check to make sure  the frame is OK.  Bail if not.
        if (StackFrame.AddrFrame.Offset == 0)
            break;

        if (MachineType == IMAGE_FILE_MACHINE_I386) {
            lprintf(
                "%08lX %08lX %08lX %08lX",
                (DWORD)StackFrame.AddrPC.Offset,
                (DWORD)StackFrame.Params[0],
                (DWORD)StackFrame.Params[1],
                (DWORD)StackFrame.Params[2]
            );
        } else {
            lprintf(
                "%016I64X %016I64X %016I64X %016I64X",
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
        char szModule[MAX_PATH];
        if (hModule &&
            GetModuleFileNameExA(hProcess, hModule, szModule, MAX_PATH)) {

            lprintf( "  %s", getBaseName(szModule));

            bSymbol = GetSymFromAddr(hProcess, AddrPC + nudge, szSymName, MAX_SYM_NAME_SIZE);
            if (bSymbol) {
                UnDecorateSymbolName(szSymName, szSymName, MAX_SYM_NAME_SIZE, UNDNAME_COMPLETE);

                lprintf( "!%s", szSymName);

                bLine = GetLineFromAddr(hProcess, AddrPC + nudge, szFileName, MAX_PATH, &dwLineNumber);
                if (bLine) {
                    lprintf( "  [%s @ %ld]", szFileName, dwLineNumber);
                }
            } else {
                lprintf( "!0x%I64x", AddrPC - (DWORD)(INT_PTR)hModule);
            }
        }

        lprintf("\r\n");

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

    lprintf("\r\n");
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
 *
 * See also:
 * - https://msdn.microsoft.com/en-us/library/windows/hardware/ff558784.aspx
 */
static LPCSTR
getExceptionString(DWORD ExceptionCode)
{
    switch (ExceptionCode) {

    case EXCEPTION_ACCESS_VIOLATION: // 0xC0000005
        return "Access Violation";
    case EXCEPTION_IN_PAGE_ERROR: // 0xC0000006
        return "In Page Error";
    case EXCEPTION_INVALID_HANDLE: // 0xC0000008
        return "Invalid Handle";
    case EXCEPTION_ILLEGAL_INSTRUCTION: // 0xC000001D
        return "Illegal Instruction";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: // 0xC0000025
        return "Cannot Continue";
    case EXCEPTION_INVALID_DISPOSITION: // 0xC0000026
        return "Invalid Disposition";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: // 0xC000008C
        return "Array bounds exceeded";
    case EXCEPTION_FLT_DENORMAL_OPERAND: // 0xC000008D
        return "Floating-point denormal operand";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: // 0xC000008E
        return "Floating-point division by zero";
    case EXCEPTION_FLT_INEXACT_RESULT: // 0xC000008F
        return "Floating-point inexact result";
    case EXCEPTION_FLT_INVALID_OPERATION: // 0xC0000090
        return "Floating-point invalid operation";
    case EXCEPTION_FLT_OVERFLOW: // 0xC0000091
        return "Floating-point overflow";
    case EXCEPTION_FLT_STACK_CHECK: // 0xC0000092
        return "Floating-point stack check";
    case EXCEPTION_FLT_UNDERFLOW: // 0xC0000093
        return "Floating-point underflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: // 0xC0000094
        return "Integer division by zero";
    case EXCEPTION_INT_OVERFLOW:  // 0xC0000095
        return "Integer overflow";
    case EXCEPTION_PRIV_INSTRUCTION: // 0xC0000096
        return "Privileged instruction";
    case EXCEPTION_STACK_OVERFLOW: // 0xC00000FD
        return "Stack Overflow";
    case EXCEPTION_POSSIBLE_DEADLOCK: // 0xC0000194
        return "Possible deadlock condition";
    case STATUS_ASSERTION_FAILURE: // 0xC0000420
        return "Assertion failure";

    case STATUS_CLR_EXCEPTION: // 0xE0434f4D
        return "CLR exception";
    case STATUS_CPP_EH_EXCEPTION: // 0xE06D7363
        return "C++ exception handling exception";

    case EXCEPTION_GUARD_PAGE: // 0x80000001
        return "Guard Page Exception";
    case EXCEPTION_DATATYPE_MISALIGNMENT: // 0x80000002
        return "Alignment Fault";
    case EXCEPTION_BREAKPOINT: // 0x80000003
        return "Breakpoint";
    case EXCEPTION_SINGLE_STEP: // 0x80000004
        return "Single Step";

    case STATUS_WX86_BREAKPOINT: // 0x4000001F
        return "Breakpoint";
    case DBG_TERMINATE_THREAD: // 0x40010003
        return "Terminate Thread";
    case DBG_TERMINATE_PROCESS: // 0x40010004
        return "Terminate Process";
    case DBG_CONTROL_C: // 0x40010005
        return "Control+C";
    case DBG_CONTROL_BREAK: // 0x40010008
        return "Control+Break";
    case 0x406D1388:
        return "Thread Name Exception";

    case RPC_S_UNKNOWN_IF:
        return "Unknown Interface";
    case RPC_S_SERVER_UNAVAILABLE:
        return "Server Unavailable";

    default:
        return NULL;
    }
}


void
dumpException(HANDLE hProcess,
              PEXCEPTION_RECORD pExceptionRecord)
{
    DWORD ExceptionCode = pExceptionRecord->ExceptionCode;

    char szModule[MAX_PATH];
    LPCSTR lpcszProcess;
    HMODULE hModule;

    if (GetModuleFileNameExA(hProcess, NULL, szModule, MAX_PATH)) {
        lpcszProcess = getBaseName(szModule);
    } else {
        lpcszProcess = "Application";
    }

    // First print information about the type of fault
    lprintf("%s caused", lpcszProcess);

    LPCSTR lpcszException = getExceptionString(ExceptionCode);
    if (lpcszException) {
        LPCSTR lpszArticle;
        switch (lpcszException[0]) {
        case 'A':
        case 'E':
        case 'I':
        case 'O':
        case 'U':
            lpszArticle = "an";
            break;
        default:
            lpszArticle = "a";
            break;
        }

        lprintf(" %s %s", lpszArticle, lpcszException);
    } else {
        lprintf(" an Unknown [0x%lX] Exception", ExceptionCode);
    }

    // Now print information about where the fault occured
    lprintf(" at location %p", pExceptionRecord->ExceptionAddress);
    if((hModule = (HMODULE)(INT_PTR)SymGetModuleBase64(hProcess, (DWORD64)(INT_PTR)pExceptionRecord->ExceptionAddress)) &&
       GetModuleFileNameExA(hProcess, hModule, szModule, sizeof szModule))
        lprintf(" in module %s", getBaseName(szModule));

    // If the exception was an access violation, print out some additional information, to the error log and the debugger.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363082%28v=vs.85%29.aspx
    if ((ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
         ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        pExceptionRecord->NumberParameters >= 2) {
        LPCSTR lpszVerb;
        switch (pExceptionRecord->ExceptionInformation[0]) {
        case 0:
            lpszVerb = "Reading from";
            break;
        case 1:
            lpszVerb = "Writing to";
            break;
        case 8:
            lpszVerb = "DEP violation at";
            break;
        default:
            lpszVerb = "Accessing";
            break;
        }

        lprintf(" %s location %p", lpszVerb, (PVOID)pExceptionRecord->ExceptionInformation[1]);
    }

    lprintf(".\r\n\r\n");
}


static BOOL
dumpSourceCode(LPCSTR lpFileName, DWORD dwLineNumber)
{
    FILE *fp;
    int i;
    char szFileName[MAX_PATH] = "";
    DWORD dwContext = 2;

    if(lpFileName[0] == '/' && lpFileName[1] == '/')
    {
        szFileName[0] = lpFileName[2];
        szFileName[1] = ':';
        strcpy(szFileName + 2, lpFileName + 3);
    }
    else
        strcpy(szFileName, lpFileName);

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


static BOOL
getModuleVersionInfo(LPCSTR szModule, DWORD *dwVInfo)
{
    DWORD dummy, size;
    BOOL success = FALSE;

    size = GetFileVersionInfoSizeA(szModule, &dummy);
    if (size > 0) {
        LPVOID pVer = malloc(size);
        ZeroMemory(pVer, size);
        if (GetFileVersionInfoA(szModule, 0, size, pVer)) {
            VS_FIXEDFILEINFO *ffi;
            if (VerQueryValueA(pVer, "\\", (LPVOID *) &ffi,  (UINT *) &dummy)) {
                dwVInfo[0] = ffi->dwFileVersionMS >> 16;
                dwVInfo[1] = ffi->dwFileVersionMS & 0xFFFF;
                dwVInfo[2] = ffi->dwFileVersionLS >> 16;
                dwVInfo[3] = ffi->dwFileVersionLS & 0xFFFF;
                success = TRUE;
            }
        }
        free(pVer);
    }
    return success;
}


void
dumpModules(HANDLE hProcess)
{

    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetProcessId(hProcess));
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    MODULEENTRY32 me32;
    me32.dwSize = sizeof me32;
    if (Module32First(hModuleSnap, &me32)) {
        do  {
            const char *szBaseName = getBaseName(me32.szExePath);
            DWORD dwVInfo[4];
            if (getModuleVersionInfo(me32.szExePath, dwVInfo)) {
                lprintf(
                    "%-12s\t%lu.%lu.%lu.%lu\n",
                    szBaseName,
                    dwVInfo[0],
                    dwVInfo[1],
                    dwVInfo[2],
                    dwVInfo[3]
                );
            } else {
                lprintf( "%s\n", szBaseName);
            }
        } while (Module32Next(hModuleSnap, &me32));
        lprintf("\n");
    }

    CloseHandle(hModuleSnap);
}
