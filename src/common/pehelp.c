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
#include <psapi.h>

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

DWORD64
PEGetImageBase(const char *szImageName)
{
    HANDLE hFile;
    HANDLE hFileMapping;
    PBYTE lpFileBase;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNtHeaders;
    PIMAGE_OPTIONAL_HEADER pOptionalHeader;
    PIMAGE_OPTIONAL_HEADER32 pOptionalHeader32;
    PIMAGE_OPTIONAL_HEADER64 pOptionalHeader64;
    DWORD64 ImageBase = 0;

    hFile = CreateFile(szImageName, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE) {
        goto no_file;
    }

    hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hFileMapping) {
        goto no_file_mapping;
    }

    lpFileBase = (PBYTE)MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (!lpFileBase) {
        goto no_view_of_file;
    }

    pDosHeader = (PIMAGE_DOS_HEADER)lpFileBase;

    pNtHeaders = (PIMAGE_NT_HEADERS)(lpFileBase + pDosHeader->e_lfanew);

    pOptionalHeader = &pNtHeaders->OptionalHeader;
    pOptionalHeader32 = (PIMAGE_OPTIONAL_HEADER32)pOptionalHeader;
    pOptionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptionalHeader;

    switch (pOptionalHeader->Magic) {
    case IMAGE_NT_OPTIONAL_HDR32_MAGIC :
        ImageBase = pOptionalHeader32->ImageBase;
        break;
    case IMAGE_NT_OPTIONAL_HDR64_MAGIC :
        ImageBase = pOptionalHeader64->ImageBase;
        break;
    default:
        assert(0);
    }

no_view_of_file:
    CloseHandle(hFileMapping);
no_file_mapping:
    CloseHandle(hFile);
no_file:
    return ImageBase;
}
