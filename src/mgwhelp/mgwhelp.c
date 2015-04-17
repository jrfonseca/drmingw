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
#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>

#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include "outdbg.h"

#include "mgwhelp.h"

#include "dwarf_pe.h"
#include "dwarf_find.h"


extern char *
__cxa_demangle(const char * __mangled_name, char * __output_buffer,
               size_t * __length, int * __status);


struct mgwhelp_module
{
    struct mgwhelp_module *next;

    DWORD64 Base;
    char LoadedImageName[MAX_PATH];

    DWORD64 image_base_vma;

    Dwarf_Debug dbg;
};


struct mgwhelp_process
{
    struct mgwhelp_process *next;

    HANDLE hProcess;

    struct mgwhelp_module *modules;
};


struct mgwhelp_process *processes = NULL;


/* We must use a memory map of the file, not read memory directly, as the
 * value of ImageBase in memory changes.
 */
static DWORD64
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


static struct mgwhelp_module *
mgwhelp_module_create(struct mgwhelp_process * process, DWORD64 Base)
{
    struct mgwhelp_module *module;
    DWORD dwRet;

    module = (struct mgwhelp_module *)calloc(1, sizeof *module);
    if (!module) {
        goto no_module;
    }

    module->Base = Base;

    /* SymGetModuleInfo64 is not reliable for this, as explained in
     * https://msdn.microsoft.com/en-us/library/windows/desktop/ms681336.aspx
     */
    dwRet = GetModuleFileNameExA(process->hProcess,
                                 (HMODULE)(UINT_PTR)Base,
                                 module->LoadedImageName,
                                 sizeof module->LoadedImageName);
    if (dwRet == 0) {
        OutputDebug("MGWHELP: could not determined module name\n");
        goto no_module_name;
    }

    module->image_base_vma = PEGetImageBase(module->LoadedImageName);

    Dwarf_Error error = 0;
    if (dwarf_pe_init(module->LoadedImageName, 0, 0, &module->dbg, &error) != DW_DLV_OK) {
        OutputDebug("MGWHELP: %s: %s\n", module->LoadedImageName, "no dwarf symbols");
    }

    module->next = process->modules;
    process->modules = module;

    return module;

no_module_name:
    free(module);
no_module:
    return NULL;
}


static void
mgwhelp_module_destroy(struct mgwhelp_module * module)
{
    if (module->dbg) {
        Dwarf_Error error = 0;
        dwarf_pe_finish(module->dbg, &error);
    }

    free(module);
}


static struct mgwhelp_module *
mgwhelp_module_lookup(struct mgwhelp_process * process, DWORD64 Base)
{
    struct mgwhelp_module *module;

    module = process->modules;
    while (module) {
        if (module->Base == Base)
            return module;

        module = module->next;
    }

    return mgwhelp_module_create(process, Base);
}


static struct mgwhelp_process *
mgwhelp_process_lookup(HANDLE hProcess)
{
    struct mgwhelp_process *process;

    process = processes;
    while (process) {
        if (process->hProcess == hProcess)
            return process;

        process = process->next;
    }

    return NULL;
}


static BOOL
mgwhelp_find_symbol(HANDLE hProcess, DWORD64 Address, struct find_dwarf_info *info)
{
    DWORD64 Base;
    struct mgwhelp_process *process;
    struct mgwhelp_module *module;

    process = mgwhelp_process_lookup(hProcess);
    if (!process) {
        return FALSE;
    }

    Base = MgwSymGetModuleBase64(hProcess, Address);
    if (!Base) {
        return FALSE;
    }

    module = mgwhelp_module_lookup(process, Base);
    if (!module)
        return FALSE;

    DWORD64 Offset = module->image_base_vma + Address - (DWORD64)module->Base;

    memset(info, 0, sizeof *info);

    if (module->dbg) {
        find_dwarf_symbol(module->dbg, Offset, info);
        if (info->found) {
            return TRUE;
        }
    }

    return FALSE;
}


static void
mgwhelp_initialize(HANDLE hProcess)
{
    struct mgwhelp_process *process;

    process = (struct mgwhelp_process *)calloc(1, sizeof *process);
    if (process) {
        process->hProcess = hProcess;

        process->next = processes;
        processes = process;
    }
}


BOOL WINAPI
MgwSymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess)
{
    BOOL ret;

    ret = SymInitialize(hProcess, UserSearchPath, fInvadeProcess);

    if (ret) {
        mgwhelp_initialize(hProcess);
    }

    return ret;
}


BOOL WINAPI
MgwSymInitializeW(HANDLE hProcess, PCWSTR UserSearchPath, BOOL fInvadeProcess)
{
    BOOL ret;

    ret = SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);

    if (ret) {
        mgwhelp_initialize(hProcess);
    }

    return ret;
}


DWORD WINAPI
MgwSymSetOptions(DWORD SymOptions)
{
    return SymSetOptions(SymOptions);
}


/*
 * The GetModuleBase64 function retrieves the base address of the module that
 * contains the specified address.
 *
 * Same as SymGetModuleBase64, but that seems to often cause problems.
 */
DWORD64 WINAPI
MgwSymGetModuleBase64(HANDLE hProcess, DWORD64 dwAddress)
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


BOOL WINAPI
MgwSymFromAddr(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol)
{
    struct find_dwarf_info info;

    if (mgwhelp_find_symbol(hProcess, Address, &info)) {
        strncpy(Symbol->Name, info.functionname, Symbol->MaxNameLen);

        if (Displacement) {
            /* TODO */
            *Displacement = 0;
        }

        return TRUE;
    }

    return SymFromAddr(hProcess, Address, Displacement, Symbol);
}


BOOL WINAPI
MgwSymGetLineFromAddr64(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line)
{
    struct find_dwarf_info info;

    if (mgwhelp_find_symbol(hProcess, dwAddr, &info)) {
        Line->FileName = (char *)info.filename;
        Line->LineNumber = info.line;

        if (pdwDisplacement) {
            /* TODO */
            *pdwDisplacement = 0;
        }

        return TRUE;
    }

    return SymGetLineFromAddr64(hProcess, dwAddr, pdwDisplacement, Line);
}


DWORD WINAPI
MgwUnDecorateSymbolName(PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags)
{
    assert(DecoratedName != NULL);

    if (DecoratedName[0] == '_' && DecoratedName[1] == 'Z') {
        /**
         * See http://gcc.gnu.org/onlinedocs/libstdc++/manual/ext_demangling.html
         */

        int status = 0;
        char *output_buffer;
        output_buffer = __cxa_demangle(DecoratedName, 0, 0, &status);
        if (status != 0) {
            OutputDebug("MGWHELP: __cxa_demangle failed with status %i\n", status);
        } else {
            strncpy(UnDecoratedName, output_buffer, UndecoratedLength);
            free(output_buffer);
            return strlen(UnDecoratedName);
        }
    }

    return UnDecorateSymbolName(DecoratedName, UnDecoratedName, UndecoratedLength, Flags);
}


BOOL WINAPI
MgwSymCleanup(HANDLE hProcess)
{
    struct mgwhelp_process **link;
    struct mgwhelp_process *process;
    struct mgwhelp_module *module;

    link = &processes;
    process = *link;
    while (process) {
        if (process->hProcess == hProcess) {
            module = process->modules;
            while (module) {
                struct mgwhelp_module *next = module->next;

                mgwhelp_module_destroy(module);

                module = next;
            }

            *link = process->next;
            free(process);
            break;
        }

        link = &process->next;
        process = *link;
    }

    return SymCleanup(hProcess);
}


// Unicode stubs


BOOL WINAPI
MgwSymFromAddrW(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFOW SymbolW)
{
    PSYMBOL_INFO SymbolA = (PSYMBOL_INFO)malloc(offsetof(SYMBOL_INFO, Name) + SymbolW->MaxNameLen);
    memcpy(SymbolA, SymbolW, offsetof(SYMBOL_INFO, Name));
    if (MgwSymFromAddr(hProcess, Address, Displacement, SymbolA)) {
        MultiByteToWideChar(CP_ACP, 0, SymbolA->Name, -1, SymbolW->Name, SymbolW->MaxNameLen);
        return TRUE;
    } else {
        return FALSE;
    }
}


BOOL WINAPI
MgwSymGetLineFromAddrW64(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINEW64 LineW)
{
    IMAGEHLP_LINE64 LineA;
    if (MgwSymGetLineFromAddr64(hProcess, dwAddr, pdwDisplacement, &LineA)) {
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms681330.aspx
        // states that SymGetLineFromAddrW64 "returns a pointer to a buffer
        // that may be reused by another function" and that callers should be
        // "sure to copy the data returned to another buffer immediately",
        // therefore the static buffer should be safe.
        static WCHAR FileName[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, LineA.FileName, -1, FileName, MAX_PATH);
        memcpy(LineW, &LineA, sizeof LineA);
        LineW->FileName = FileName;
        return TRUE;
    } else {
        return FALSE;
    }
}
