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
#include <stddef.h>

#include <windows.h>

#include "misc.h"
#include "dbghelp.h"
#include "pehelp.h"


/*
 * The GetModuleBase64 function retrieves the base address of the module that
 * contains the specified address.
 *
 * Same as SymGetModuleBase64, but that seems to often cause problems.
 */
DWORD64 WINAPI
GetModuleBase64(HANDLE hProcess, DWORD64 dwAddress)
{
    if (hProcess == GetCurrentProcess()) {
        HMODULE hModule = NULL;
        BOOL bRet = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                      (LPCTSTR)(UINT_PTR)dwAddress,
                                      &hModule);
        if (bRet) {
            return (DWORD64)(UINT_PTR)hModule;
        }
    }

    MEMORY_BASIC_INFORMATION Buffer;
    if (VirtualQueryEx(hProcess, (LPCVOID)(UINT_PTR)dwAddress, &Buffer, sizeof Buffer) != 0) {
        return (DWORD64)(UINT_PTR)Buffer.AllocationBase;
    }

    return SymGetModuleBase64(hProcess, dwAddress);
}

BOOL CALLBACK
ReadProcessMemory64(HANDLE hProcess, DWORD64 lpBaseAddress, PVOID lpBuffer, DWORD nSize, PDWORD lpNumberOfBytesRead)
{
    SIZE_T NumberOfBytesRead = 0;
    BOOL bRet = ReadProcessMemory(hProcess, (LPCVOID)(UINT_PTR)lpBaseAddress, lpBuffer, nSize, &NumberOfBytesRead);
    if (lpNumberOfBytesRead) {
        *lpNumberOfBytesRead = NumberOfBytesRead;
    }
    return bRet;
}

DWORD64
PEImageNtHeader(HANDLE hProcess, DWORD64 hModule)
{
    LONG e_lfanew;

    // From the DOS header, find the NT (PE) header
    if (!ReadProcessMemory64(hProcess, hModule + offsetof(IMAGE_DOS_HEADER, e_lfanew), &e_lfanew, sizeof e_lfanew, NULL))
        return 0;

    return hModule + e_lfanew;
}

DWORD64
PEGetImageBase(HANDLE hProcess, DWORD64 hModule)
{
    DWORD64 pNtHeaders;
    IMAGE_NT_HEADERS NtHeaders;

    if(!(pNtHeaders = PEImageNtHeader(hProcess, hModule)))
        return FALSE;

    if(!ReadProcessMemory64(hProcess, pNtHeaders, &NtHeaders, sizeof NtHeaders, NULL))
        return FALSE;

    return NtHeaders.OptionalHeader.ImageBase;
}
