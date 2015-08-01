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

    HANDLE hFileMapping;
    PBYTE lpFileBase;

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
PEGetImageBase(PBYTE lpFileBase)
{
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNtHeaders;
    PIMAGE_OPTIONAL_HEADER pOptionalHeader;
    PIMAGE_OPTIONAL_HEADER32 pOptionalHeader32;
    PIMAGE_OPTIONAL_HEADER64 pOptionalHeader64;
    DWORD64 ImageBase = 0;

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

    return ImageBase;
}


/*
 * Search for the symbol on PE's symbol table.
 *
 * Symbols for which there's no DWARF debugging information might still appear there, put by MinGW linker.
 *
 * - https://msdn.microsoft.com/en-gb/library/ms809762.aspx
 *   - https://www.microsoft.com/msj/backissues86.aspx
 * - http://go.microsoft.com/fwlink/p/?linkid=84140
 */
static BOOL
pe_find_symbol(PBYTE lpFileBase,
               DWORD64 Addr,
               ULONG MaxSymbolNameLen,
               LPSTR pSymbolName,
               PDWORD64 pDisplacement)
{
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNtHeaders;
    PIMAGE_OPTIONAL_HEADER pOptionalHeader;
    PIMAGE_OPTIONAL_HEADER32 pOptionalHeader32;
    PIMAGE_OPTIONAL_HEADER64 pOptionalHeader64;
    DWORD64 ImageBase = 0;
    BOOL bUnderscore = TRUE;

    pDosHeader = (PIMAGE_DOS_HEADER)lpFileBase;
    pNtHeaders = (PIMAGE_NT_HEADERS)(lpFileBase + pDosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER Sections = (PIMAGE_SECTION_HEADER) (
        (PBYTE)pNtHeaders +
        sizeof(DWORD) +
        sizeof(IMAGE_FILE_HEADER) +
        pNtHeaders->FileHeader.SizeOfOptionalHeader
    );
    PIMAGE_SYMBOL pSymbolTable = (PIMAGE_SYMBOL) (
        lpFileBase +
        pNtHeaders->FileHeader.PointerToSymbolTable
    );
    PSTR pStringTable = (PSTR)
        &pSymbolTable[pNtHeaders->FileHeader.NumberOfSymbols];
    pOptionalHeader = &pNtHeaders->OptionalHeader;
    pOptionalHeader32 = (PIMAGE_OPTIONAL_HEADER32)pOptionalHeader;
    pOptionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptionalHeader;

    switch (pOptionalHeader->Magic) {
    case IMAGE_NT_OPTIONAL_HDR32_MAGIC :
        ImageBase = pOptionalHeader32->ImageBase;
        break;
    case IMAGE_NT_OPTIONAL_HDR64_MAGIC :
        ImageBase = pOptionalHeader64->ImageBase;
        bUnderscore = FALSE;
        break;
    default:
        assert(0);
    }

    DWORD64 Displacement = ~(DWORD64)0;
    BOOL bRet = FALSE;

    DWORD i;
    for (i = 0; i < pNtHeaders->FileHeader.NumberOfSymbols; ++i) {
        PIMAGE_SYMBOL pSymbol = &pSymbolTable[i];

        DWORD64 SymbolAddr = pSymbol->Value;
        SHORT SectionNumber = pSymbol->SectionNumber;
        if (SectionNumber > 0) {
            PIMAGE_SECTION_HEADER pSection = Sections + SectionNumber - 1;
            SymbolAddr += ImageBase + pSection->VirtualAddress;
        }

        LPCSTR SymbolName;
        char ShortName[9];
        if (pSymbol->N.Name.Short != 0) {
            strncpy(ShortName, (LPCSTR)pSymbol->N.ShortName, 8);
            ShortName[8] = '\0';
            SymbolName = ShortName;
        } else {
            SymbolName = &pStringTable[pSymbol->N.Name.Long];
        }

        if (bUnderscore && SymbolName[0] == '_') {
            SymbolName = &SymbolName[1];
        }

        if (0) {
            OutputDebug("%04lu: 0x%08I64X %s\n", i, SymbolAddr, SymbolName);
        }

        if (SymbolAddr <= Addr &&
            ISFCN(pSymbol->Type) &&
            SymbolName[0] != '.') {
            DWORD64 SymbolDisp = Addr - SymbolAddr;
            if (SymbolDisp < Displacement) {
                strncpy(pSymbolName, SymbolName, MaxSymbolNameLen);

                bRet = TRUE;

                Displacement = SymbolDisp;

                if (Displacement == 0) {
                    break;
                }
            }
        }

        i += pSymbol->NumberOfAuxSymbols;
    }

    if (pDisplacement) {
        *pDisplacement = Displacement;
    }

    return bRet;
}


static struct mgwhelp_module *
mgwhelp_module_create(struct mgwhelp_process * process,
                      HANDLE hFile,
                      PCSTR ImageName,
                      DWORD64 Base)
{
    struct mgwhelp_module *module;
    BOOL bOwnFile;

    module = (struct mgwhelp_module *)calloc(1, sizeof *module);
    if (!module) {
        goto no_module;
    }

    module->Base = Base;

    if (ImageName) {
        strncpy(module->LoadedImageName, ImageName, sizeof module->LoadedImageName);
    } else {
        /* SymGetModuleInfo64 is not reliable for this, as explained in
         * https://msdn.microsoft.com/en-us/library/windows/desktop/ms681336.aspx
         */
        DWORD dwRet;
        dwRet = GetModuleFileNameExA(process->hProcess,
                                     (HMODULE)(UINT_PTR)Base,
                                     module->LoadedImageName,
                                     sizeof module->LoadedImageName);
        if (dwRet == 0) {
            OutputDebug("MGWHELP: could not determine module name\n");
            goto no_module_name;
        }
    }

    bOwnFile = FALSE;
    if (!hFile) {
        hFile = CreateFileA(module->LoadedImageName, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (hFile == INVALID_HANDLE_VALUE) {
            OutputDebug("MGWHELP: %s - file not found\n", module->LoadedImageName);
            goto no_module_name;
        }
        bOwnFile = TRUE;
    }

    module->hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!module->hFileMapping) {
        goto no_file_mapping;
    }

    module->lpFileBase = (PBYTE)MapViewOfFile(module->hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (!module->lpFileBase) {
        goto no_view_of_file;
    }

    module->image_base_vma = PEGetImageBase(module->lpFileBase);

    Dwarf_Error error = 0;
    if (dwarf_pe_init(hFile, module->LoadedImageName, 0, 0, &module->dbg, &error) != DW_DLV_OK) {
        /* do nothing */
    }

    if (bOwnFile) {
        CloseHandle(hFile);
    }

    module->next = process->modules;
    process->modules = module;

    return module;

no_view_of_file:
    CloseHandle(module->hFileMapping);
no_file_mapping:
    if (bOwnFile) {
        CloseHandle(hFile);
    }
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


static struct mgwhelp_process *
mgwhelp_process_lookup(HANDLE hProcess);

static struct mgwhelp_module *
mgwhelp_module_lookup(HANDLE hProcess,
                      HANDLE hFile,
                      PCSTR ImageName,
                      DWORD64 Base)
{
    struct mgwhelp_process *process;
    struct mgwhelp_module *module;

    process = mgwhelp_process_lookup(hProcess);
    if (!process) {
        return NULL;
    }

    module = process->modules;
    while (module) {
        if (module->Base == Base)
            return module;

        module = module->next;
    }

    return mgwhelp_module_create(process, hFile, ImageName, Base);
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


static struct mgwhelp_module *
mgwhelp_find_module(HANDLE hProcess, DWORD64 Address, PDWORD64 pOffset)
{
    DWORD64 Base;
    struct mgwhelp_module *module;

    Base = MgwSymGetModuleBase64(hProcess, Address);
    if (!Base) {
        return FALSE;
    }

    module = mgwhelp_module_lookup(hProcess, 0, NULL, Base);
    if (!module) {
        return NULL;
    }

    *pOffset = module->image_base_vma + Address - (DWORD64)module->Base;

    return module;
}


static BOOL
mgwhelp_find_symbol(HANDLE hProcess, DWORD64 Address, struct find_dwarf_info *info)
{
    struct mgwhelp_module *module;

    DWORD64 Offset;
    module = mgwhelp_find_module(hProcess, Address, &Offset);
    if (!module) {
        return FALSE;
    }

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


DWORD64 WINAPI
MgwSymLoadModuleEx(HANDLE hProcess,
                   HANDLE hFile,
                   PCSTR ImageName,
                   PCSTR ModuleName,
                   DWORD64 BaseOfDll,
                   DWORD DllSize,
                   PMODLOAD_DATA Data,
                   DWORD Flags)
{
    DWORD dwRet;

    dwRet = SymLoadModuleEx(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);

    if (BaseOfDll) {
        mgwhelp_module_lookup(hProcess, hFile, ImageName, BaseOfDll);
    }

    return dwRet;
}


DWORD64 WINAPI
MgwSymLoadModuleExW(HANDLE hProcess,
                    HANDLE hFile,
                    PCWSTR ImageName,
                    PCWSTR ModuleName,
                    DWORD64 BaseOfDll,
                    DWORD DllSize,
                    PMODLOAD_DATA Data,
                    DWORD Flags)
{
    DWORD dwRet;

    dwRet = SymLoadModuleExW(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);

    if (BaseOfDll) {
        char ImageNameBuf[MAX_PATH];
        PCSTR ImageNameA;

        if (ImageName) {
            WideCharToMultiByte(CP_ACP, 0, ImageName, -1, ImageNameBuf, _countof(ImageNameBuf), NULL, NULL);
            ImageNameA = ImageNameBuf;
        } else {
            ImageNameA = NULL;
        }

        mgwhelp_module_lookup(hProcess, hFile, ImageNameA, BaseOfDll);
    }

    return dwRet;
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
        BOOL bRet = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)(UINT_PTR)dwAddress,
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


static char *
demangle(const char *mangled)
{
    assert(mangled);
    if (mangled[0] != '_' || mangled[1] != 'Z') {
        return NULL;
    }

    /**
     * See http://gcc.gnu.org/onlinedocs/libstdc++/manual/ext_demangling.html
     */

    int status = 0;
    char *output_buffer;
    output_buffer = __cxa_demangle(mangled, 0, 0, &status);
    if (status != 0) {
        OutputDebug("MGWHELP: __cxa_demangle failed with status %i\n", status);
        return NULL;
    } else {
        return output_buffer;
    }
}


BOOL WINAPI
MgwSymFromAddr(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol)
{
    struct find_dwarf_info info;

    DWORD dwOptions = SymGetOptions();

    if (mgwhelp_find_symbol(hProcess, Address, &info)) {
        char *output_buffer = NULL;
        if (dwOptions & SYMOPT_UNDNAME) {
            output_buffer = demangle(info.functionname);
        }
        if (output_buffer) {
            strncpy(Symbol->Name, output_buffer, Symbol->MaxNameLen);
            free(output_buffer);
        } else {
            strncpy(Symbol->Name, info.functionname, Symbol->MaxNameLen);
        }

        if (Displacement) {
            /* TODO */
            *Displacement = 0;
        }

        return TRUE;
    }

    struct mgwhelp_module *module;
    DWORD64 Offset;
    module = mgwhelp_find_module(hProcess, Address, &Offset);
    if (module && module->lpFileBase) {
        if (pe_find_symbol(module->lpFileBase,
                           Offset,
                           Symbol->MaxNameLen,
                           Symbol->Name,
                           Displacement)) {
            char *output_buffer = NULL;
            if (dwOptions & SYMOPT_UNDNAME) {
                output_buffer = demangle(Symbol->Name);
            }
            if (output_buffer) {
                strncpy(Symbol->Name, output_buffer, Symbol->MaxNameLen);
                free(output_buffer);
            }
            return TRUE;
        }
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

    char *output_buffer = demangle(DecoratedName);
    if (output_buffer) {
        strncpy(UnDecoratedName, output_buffer, UndecoratedLength);
        free(output_buffer);
        return strlen(UnDecoratedName);
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
