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

#include "demangle.h"


struct mgwhelp_module {
    struct mgwhelp_module *next;

    DWORD64 Base;
    char LoadedImageName[MAX_PATH];

    HANDLE hFileMapping;
    PBYTE lpFileBase;
    SIZE_T nFileSize;

    DWORD64 image_base_vma;

    dwarf_module dwarf;
};


struct mgwhelp_process {
    struct mgwhelp_process *next;

    HANDLE hProcess;

    struct mgwhelp_module *modules;
};


struct mgwhelp_process *processes = NULL;


static DWORD64 WINAPI
GetModuleBase(HANDLE hProcess, DWORD64 dwAddress);


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
    case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
        ImageBase = pOptionalHeader32->ImageBase;
        break;
    case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
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
 * Symbols for which there's no DWARF debugging information might still appear there, put by MinGW
 * linker.
 *
 * - https://msdn.microsoft.com/en-gb/library/ms809762.aspx
 *   - https://www.microsoft.com/msj/backissues86.aspx
 * - http://go.microsoft.com/fwlink/p/?linkid=84140
 */
static BOOL
pe_find_symbol(struct mgwhelp_module *module,
               DWORD64 Addr,
               ULONG MaxSymbolNameLen,
               LPSTR pSymbolName,
               PDWORD64 pDisplacement)
{
    PBYTE lpFileBase = module->lpFileBase;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNtHeaders;
    PIMAGE_OPTIONAL_HEADER pOptionalHeader;
    PIMAGE_OPTIONAL_HEADER32 pOptionalHeader32;
    PIMAGE_OPTIONAL_HEADER64 pOptionalHeader64;
    DWORD64 ImageBase = 0;
    BOOL bUnderscore = TRUE;

    pDosHeader = (PIMAGE_DOS_HEADER)lpFileBase;
    pNtHeaders = (PIMAGE_NT_HEADERS)(lpFileBase + pDosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER Sections =
        (PIMAGE_SECTION_HEADER)((PBYTE)pNtHeaders + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                                pNtHeaders->FileHeader.SizeOfOptionalHeader);
    PIMAGE_SYMBOL pSymbolTable =
        (PIMAGE_SYMBOL)(lpFileBase + pNtHeaders->FileHeader.PointerToSymbolTable);
    PSTR pStringTable = (PSTR)&pSymbolTable[pNtHeaders->FileHeader.NumberOfSymbols];
    pOptionalHeader = &pNtHeaders->OptionalHeader;
    pOptionalHeader32 = (PIMAGE_OPTIONAL_HEADER32)pOptionalHeader;
    pOptionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptionalHeader;

    if (pNtHeaders->FileHeader.PointerToSymbolTable +
            pNtHeaders->FileHeader.NumberOfSymbols * sizeof pSymbolTable[0] >
        module->nFileSize) {
        OutputDebug("MGWHELP: %s - symbol table extends beyond image size\n",
                    module->LoadedImageName);
        return FALSE;
    }

    switch (pOptionalHeader->Magic) {
    case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
        ImageBase = pOptionalHeader32->ImageBase;
        break;
    case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
        ImageBase = pOptionalHeader64->ImageBase;
        bUnderscore = FALSE;
        break;
    default:
        assert(0);
        return FALSE;
    }

    DWORD64 Displacement = ~(DWORD64)0;
    BOOL bRet = FALSE;

    DWORD i;
    for (i = 0; i < pNtHeaders->FileHeader.NumberOfSymbols; ++i) {
        PIMAGE_SYMBOL pSymbol = &pSymbolTable[i];

        if (ISFCN(pSymbol->Type)) {
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

            if (SymbolAddr <= Addr && SymbolName[0] != '.') {
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
        }

        i += pSymbol->NumberOfAuxSymbols;
    }

    if (pDisplacement) {
        *pDisplacement = Displacement;
    }

    return bRet;
}


static struct mgwhelp_module *
mgwhelp_module_create(struct mgwhelp_process *process, HANDLE hFile, PCSTR ImageName, DWORD64 Base)
{
    struct mgwhelp_module *module;
    BOOL bOwnFile;
    DWORD dwFileSizeHi;
    DWORD dwFileSizeLo;
    Dwarf_Error error;

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
        dwRet = GetModuleFileNameExA(process->hProcess, (HMODULE)(UINT_PTR)Base,
                                     module->LoadedImageName, sizeof module->LoadedImageName);
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

    dwFileSizeHi = 0;
    dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);
    module->nFileSize = dwFileSizeLo;
#ifdef _WIN64
    module->nFileSize |= (SIZE_T)dwFileSizeHi << 32;
#else
    assert(dwFileSizeHi == 0);
#endif

    module->image_base_vma = PEGetImageBase(module->lpFileBase);

    error = 0;
    if (dwarf_pe_init(hFile, module->LoadedImageName, 0, 0, &module->dwarf.dbg, &error) ==
        DW_DLV_OK) {
        if (dwarf_get_aranges(module->dwarf.dbg, &module->dwarf.aranges,
                              &module->dwarf.arange_count, &error) != DW_DLV_OK) {
            OutputDebug("MGWHELP: libdwarf error - %s\n", dwarf_errmsg(error));
        }
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
mgwhelp_module_destroy(struct mgwhelp_module *module)
{
    if (module->dwarf.dbg) {
        Dwarf_Error error = 0;
        if (module->dwarf.aranges) {
            for (Dwarf_Signed i = 0; i < module->dwarf.arange_count; ++i) {
                dwarf_dealloc(module->dwarf.dbg, module->dwarf.aranges[i], DW_DLA_ARANGE);
            }
            dwarf_dealloc(module->dwarf.dbg, module->dwarf.aranges, DW_DLA_LIST);
        }
        dwarf_pe_finish(module->dwarf.dbg, &error);
    }

    UnmapViewOfFile(module->lpFileBase);
    free(module);
}


static struct mgwhelp_process *
mgwhelp_process_lookup(HANDLE hProcess);

static struct mgwhelp_module *
mgwhelp_module_lookup(HANDLE hProcess, HANDLE hFile, PCSTR ImageName, DWORD64 Base)
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

    Base = GetModuleBase(hProcess, Address);
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

    dwRet =
        SymLoadModuleEx(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);

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

    dwRet =
        SymLoadModuleExW(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);

    if (BaseOfDll) {
        char ImageNameBuf[MAX_PATH];
        PCSTR ImageNameA;

        if (ImageName) {
            WideCharToMultiByte(CP_ACP, 0, ImageName, -1, ImageNameBuf, _countof(ImageNameBuf),
                                NULL, NULL);
            ImageNameA = ImageNameBuf;
        } else {
            ImageNameA = NULL;
        }

        mgwhelp_module_lookup(hProcess, hFile, ImageNameA, BaseOfDll);
    }

    return dwRet;
}


/*
 * The GetModuleBase function retrieves the base address of the module that
 * contains the specified address.
 *
 * Same as SymGetModuleBase64, but that seems to often cause problems.
 */
static DWORD64 WINAPI
GetModuleBase(HANDLE hProcess, DWORD64 dwAddress)
{
    if (hProcess == GetCurrentProcess()) {
        HMODULE hModule = NULL;
        BOOL bRet = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)(UINT_PTR)dwAddress, &hModule);
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
demangle(const char *mangled, DWORD Flags)
{
    assert(mangled);

    // There can be false negatives, such as "_ZwTerminateProcess@8"
    if (mangled[0] != '_' || mangled[1] != 'Z') {
        return NULL;
    }

    int options = DMGL_PARAMS | DMGL_TYPES;
    if (Flags & UNDNAME_NAME_ONLY) {
        options = DMGL_NO_OPTS;
    }
    if (Flags & UNDNAME_NO_ARGUMENTS) {
        options &= ~DMGL_PARAMS;
    }

    return cplus_demangle_v3(mangled, options);
}


BOOL WINAPI
MgwSymFromAddr(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol)
{
    DWORD dwOptions = SymGetOptions();

    // search pe symbols first (more accurate than dwarf)
    DWORD64 Offset;
    mgwhelp_module *module = mgwhelp_find_module(hProcess, Address, &Offset);

    if (module && module->lpFileBase) {
        if (pe_find_symbol(module, Offset, Symbol->MaxNameLen, Symbol->Name, Displacement)) {
            if (dwOptions & SYMOPT_UNDNAME) {
                char *output_buffer = demangle(Symbol->Name, UNDNAME_NAME_ONLY);
                if (output_buffer) {
                    strncpy(Symbol->Name, output_buffer, Symbol->MaxNameLen);
                    free(output_buffer);
                }
            }
            return TRUE;
        }
    }

    if (module && module->dwarf.aranges) {
        struct dwarf_symbol_info info;
        if (dwarf_find_symbol(&module->dwarf, Offset, &info)) {
            strncpy(Symbol->Name, info.functionname.c_str(), Symbol->MaxNameLen);
            if (dwOptions & SYMOPT_UNDNAME) {
                char *output_buffer = demangle(info.functionname.c_str(), UNDNAME_NAME_ONLY);
                if (output_buffer) {
                    strncpy(Symbol->Name, output_buffer, Symbol->MaxNameLen);
                    free(output_buffer);
                }
            }
            if (Displacement) {
                /* TODO */
                *Displacement = 0;
            }
            return TRUE;
        }
    }

    return SymFromAddr(hProcess, Address, Displacement, Symbol);
}


BOOL WINAPI
MgwSymGetLineFromAddr64(HANDLE hProcess,
                        DWORD64 dwAddr,
                        PDWORD pdwDisplacement,
                        PIMAGEHLP_LINE64 Line)
{
    DWORD64 Offset;
    mgwhelp_module *module = mgwhelp_find_module(hProcess, dwAddr, &Offset);

    if (module && module->dwarf.aranges) {
        static struct dwarf_line_info info;
        if (dwarf_find_line(&module->dwarf, Offset, &info)) {
            static char buf[1024];
            strncpy(buf, info.filename.c_str(), sizeof buf);
            Line->FileName = buf;
            Line->LineNumber = info.line;

            if (pdwDisplacement) {
                /* TODO */
                *pdwDisplacement = 0;
            }
            return TRUE;
        }
    }

    return SymGetLineFromAddr64(hProcess, dwAddr, pdwDisplacement, Line);
}


DWORD WINAPI
MgwUnDecorateSymbolName(PCSTR DecoratedName,
                        PSTR UnDecoratedName,
                        DWORD UndecoratedLength,
                        DWORD Flags)
{
    assert(DecoratedName != NULL);

    char *output_buffer = demangle(DecoratedName, Flags);
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
    char buffer[1024];
    PSYMBOL_INFO SymbolA = (PSYMBOL_INFO)buffer;
    SymbolA->SizeOfStruct = sizeof *SymbolA;
    SymbolA->MaxNameLen = ((sizeof(buffer) - sizeof *SymbolA) / sizeof(CHAR)) - 1;
    if (MgwSymFromAddr(hProcess, Address, Displacement, SymbolA)) {
        MultiByteToWideChar(CP_ACP, 0, SymbolA->Name, -1, SymbolW->Name, SymbolW->MaxNameLen);
        return TRUE;
    } else {
        return FALSE;
    }
}


BOOL WINAPI
MgwSymGetLineFromAddrW64(HANDLE hProcess,
                         DWORD64 dwAddr,
                         PDWORD pdwDisplacement,
                         PIMAGEHLP_LINEW64 LineW)
{
    IMAGEHLP_LINE64 LineA;
    ZeroMemory(&LineA, sizeof LineA);
    LineA.SizeOfStruct = sizeof LineA;
    if (MgwSymGetLineFromAddr64(hProcess, dwAddr, pdwDisplacement, &LineA)) {
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms681330.aspx
        // states that SymGetLineFromAddrW64 "returns a pointer to a buffer
        // that may be reused by another function" and that callers should be
        // "sure to copy the data returned to another buffer immediately",
        // therefore the static buffer should be safe.
        static WCHAR FileName[1024];
        LineW->FileName = FileName;
        MultiByteToWideChar(CP_ACP, 0, LineA.FileName, -1, LineW->FileName, 1024);
        LineW->LineNumber = LineA.LineNumber;
        return TRUE;
    } else {
        return FALSE;
    }
}
