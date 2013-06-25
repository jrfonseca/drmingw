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

#include "misc.h"
#include "demangle.h"
#include "pehelp.h"
#include "mgwhelp.h"

#include <dwarf.h>
#include <libdwarf.h>
#include "dwarf_pe.h"

#ifdef HAVE_BFD

/*
 * bfd.h will complain without this.
 */
#ifndef PACKAGE
#define PACKAGE
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION
#endif

#include <bfd.h>

#endif


struct mgwhelp_module
{
    struct mgwhelp_module *next;

    DWORD64 Base;

    IMAGEHLP_MODULE64 ModuleInfo;

    DWORD64 image_base_vma;

    Dwarf_Debug dbg;

#ifdef HAVE_BFD
    bfd *abfd;
    asymbol **syms;
    long symcount;
#endif  /* HAVE_BFD */
};


struct mgwhelp_process
{
    struct mgwhelp_process *next;

    HANDLE hProcess;

    struct mgwhelp_module *modules;
};


struct mgwhelp_process *processes = NULL;


struct find_handle
{
    struct mgwhelp_module *module;
    DWORD64 pc;
    const char *filename;
    const char *functionname;
    unsigned int line;
    bool found;
};


/*-
 * elftoolchain-0.6.1/addr2line/addr2line.c
 *
 * Copyright (c) 2009 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static char unknown[] = { '?', '?', '\0' };
static void
search_func(Dwarf_Debug dbg,
            Dwarf_Die die,
            Dwarf_Addr addr,
            char **rlt_func)
{
    Dwarf_Die ret_die, spec_die;
    Dwarf_Error de;
    Dwarf_Half tag;
    Dwarf_Unsigned lopc, hipc;
    Dwarf_Off ref;
    Dwarf_Attribute sub_at, spec_at;
    char *func0;
    int ret;

    if (*rlt_func != NULL)
        return;

    if (dwarf_tag(die, &tag, &de)) {
        OutputDebug("dwarf_tag: %s", dwarf_errmsg(de));
        goto cont_search;
    }
    if (tag == DW_TAG_subprogram) {
        if (dwarf_lowpc(die, &lopc, &de) ||
            dwarf_highpc(die, &hipc, &de))
            goto cont_search;
        if (addr < lopc || addr >= hipc)
            goto cont_search;

        /* Found it! */

        *rlt_func = unknown;
        ret = dwarf_attr(die, DW_AT_name, &sub_at, &de);
        if (ret == DW_DLV_ERROR)
            return;
        if (ret == DW_DLV_OK) {
            if (dwarf_formstring(sub_at, &func0, &de))
                *rlt_func = unknown;
            else
                *rlt_func = func0;
            return;
        }

        /*
         * If DW_AT_name is not present, but DW_AT_specification is
         * present, then probably the actual name is in the DIE
         * referenced by DW_AT_specification.
         */
        if (dwarf_attr(die, DW_AT_specification, &spec_at, &de) != DW_DLV_OK)
            return;
        if (dwarf_global_formref(spec_at, &ref, &de) != DW_DLV_OK)
            return;
        if (dwarf_offdie(dbg, ref, &spec_die, &de) != DW_DLV_OK)
            return;
        if (dwarf_diename(spec_die, rlt_func, &de) != DW_DLV_OK)
            *rlt_func = unknown;

        return;
    }

cont_search:

    /* Search children. */
    ret = dwarf_child(die, &ret_die, &de);
    if (ret == DW_DLV_ERROR)
        OutputDebug("dwarf_child: %s", dwarf_errmsg(de));
    else if (ret == DW_DLV_OK)
        search_func(dbg, ret_die, addr, rlt_func);

    /* Search sibling. */
    ret = dwarf_siblingof(dbg, die, &ret_die, &de);
    if (ret == DW_DLV_ERROR)
        OutputDebug("dwarf_siblingof: %s", dwarf_errmsg(de));
    else if (ret == DW_DLV_OK)
        search_func(dbg, ret_die, addr, rlt_func);
}

static void
find_dwarf_symbol(struct mgwhelp_module *module,
                  DWORD64 addr,
                  struct find_handle *info)
{
    Dwarf_Debug dbg = module->dbg;
    Dwarf_Error error = 0;
    char *funcname = NULL;

    Dwarf_Arange *aranges;
    Dwarf_Signed arange_count;
    if (dwarf_get_aranges(dbg, &aranges, &arange_count, &error) != DW_DLV_OK) {
        goto no_aranges;
    }

    Dwarf_Arange arange;
    if (dwarf_get_arange(aranges, arange_count, addr, &arange, &error) != DW_DLV_OK) {
        goto no_arange;
    }

    Dwarf_Off cu_die_offset;
    if (dwarf_get_cu_die_offset(arange, &cu_die_offset, &error) != DW_DLV_OK) {
        goto no_die_offset;
    }

    Dwarf_Die cu_die;
    if (dwarf_offdie_b(dbg, cu_die_offset, 1, &cu_die, &error) != DW_DLV_OK) {
        goto no_cu_die;
    }

    search_func(dbg, cu_die, addr, &funcname);
    if (funcname) {
        OutputDebug("funcname = %s!!!\n", funcname);
        info->functionname = funcname;
        info->found = true;
    }

    Dwarf_Line *linebuf;
    Dwarf_Signed linecount;
    if (dwarf_srclines(cu_die, &linebuf, &linecount, &error) == DW_DLV_OK) {
        Dwarf_Unsigned lineno, plineno;
        Dwarf_Addr lineaddr, plineaddr;
        char *file, *file0, *pfile;
        plineaddr = ~0ULL;
        plineno = lineno = 0;
        pfile = file = unknown;
        Dwarf_Signed i;
        for (i = 0; i < linecount; i++) {
            if (dwarf_lineaddr(linebuf[i], &lineaddr, &error) != DW_DLV_OK) {
                OutputDebug("dwarf_lineaddr: %s",
                    dwarf_errmsg(error));
                break;
            }
            if (addr > plineaddr && addr < lineaddr) {
                lineno = plineno;
                file = pfile;
                break;
            }
            if (dwarf_lineno(linebuf[i], &lineno, &error) != DW_DLV_OK) {
                OutputDebug("dwarf_lineno: %s",
                    dwarf_errmsg(error));
                break;
            }
            if (dwarf_linesrc(linebuf[i], &file0, &error) != DW_DLV_OK) {
                OutputDebug("dwarf_linesrc: %s",
                    dwarf_errmsg(error));
            } else {
                file = file0;
            }
            if (addr == lineaddr) {
                break;
            }
            plineaddr = lineaddr;
            plineno = lineno;
            pfile = file;
        }

        info->filename = file;
        info->line = lineno;

        dwarf_srclines_dealloc(dbg, linebuf, linecount);
    }

    dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
no_cu_die:
    ;
no_die_offset:
    ;
no_arange:
    for (Dwarf_Signed i = 0; i < arange_count; ++i) {
        dwarf_dealloc(dbg, aranges[i], DW_DLA_ARANGE);
    }
    dwarf_dealloc(dbg, aranges, DW_DLA_LIST);
no_aranges:
    if (error) {
        OutputDebug("libdwarf error: %s\n", dwarf_errmsg(error));
    }
}


#ifdef HAVE_BFD

// Read in the symbol table.
static bfd_boolean
slurp_symtab (bfd *abfd, asymbol ***syms, long *symcount)
{
    unsigned int size;

    if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
        return FALSE;

    *symcount = bfd_read_minisymbols (abfd, FALSE, (void *) syms, &size);
    if (*symcount == 0)
        *symcount = bfd_read_minisymbols (abfd, TRUE /* dynamic */, (void *) syms, &size);

    if (*symcount < 0)
        return FALSE;

    return TRUE;
}

#endif /* HAVE_BFD */


static struct mgwhelp_module *
mgwhelp_module_create(struct mgwhelp_process * process, DWORD64 Base)
{
    struct mgwhelp_module *module;

    module = (struct mgwhelp_module *)calloc(1, sizeof *module);
    if (!module)
        return NULL;

    module->Base = Base;

    module->next = process->modules;
    process->modules = module;

    module->ModuleInfo.SizeOfStruct = sizeof module->ModuleInfo;
#if 0
    if (!SymGetModuleInfo64(process->hProcess, Base, &module->ModuleInfo)) {
        OutputDebug("No module info");
        goto no_bfd;
    }
#else
    module->ModuleInfo.BaseOfImage = Base;
    if (!module->ModuleInfo.LoadedImageName[0]) {
        GetModuleFileNameExA(process->hProcess,
                             (HMODULE)(UINT_PTR)module->ModuleInfo.BaseOfImage,
                             module->ModuleInfo.LoadedImageName,
                             sizeof module->ModuleInfo.LoadedImageName);
    }
#endif


    module->image_base_vma = PEGetImageBase(process->hProcess, Base);

    Dwarf_Error error = 0;
    if (dwarf_pe_init(module->ModuleInfo.LoadedImageName, 0, 0, &module->dbg, &error) == DW_DLV_OK) {
        return module;
    } else {
        OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "no dwarf symbols");
    }

#ifdef HAVE_BFD
    module->abfd = bfd_openr(module->ModuleInfo.LoadedImageName, NULL);
    if (!module->abfd) {
        OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "could not open");
        goto no_bfd;
    }

    if (!bfd_check_format(module->abfd, bfd_object)) {
        OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "bad format");
        goto bad_format;
    }

    if (!(bfd_get_file_flags(module->abfd) & HAS_SYMS)) {
        OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "no bfd symbols");
        goto no_symbols;
    }

    if (!slurp_symtab(module->abfd, &module->syms, &module->symcount)) {
        OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "no bfd symbols");
        goto no_symbols;
    }

    if (!module->symcount) {
        OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "no bfd symbols");
        goto no_symcount;
    }

#endif /* HAVE_BFD */

    return module;

#ifdef HAVE_BFD
no_symcount:
    free(module->syms);
    module->syms = NULL;
no_symbols:
bad_format:
    bfd_close(module->abfd);
    module->abfd = NULL;
no_bfd:
    return module;
#endif /* HAVE_BFD */
}


static void
mgwhelp_module_destroy(struct mgwhelp_module * module)
{
#ifdef HAVE_BFD
    if (module->abfd) {
        if (module->syms) {
            free(module->syms);
        }

        bfd_close(module->abfd);
    }
#endif /* HAVE_BFD */

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

    process = (struct mgwhelp_process *)calloc(1, sizeof *process);
    if (!process)
        return process;

    process->hProcess = hProcess;

    process->next = processes;
    processes = process;

    return process;
}


#ifdef HAVE_BFD

// Look for an address in a section.  This is called via  bfd_map_over_sections.
static void
find_address_in_section (bfd *abfd, asection *section, void *data)
{
    struct find_handle *info = (struct find_handle *) data;
    struct mgwhelp_module *module = info->module;
    bfd_vma vma;
    bfd_size_type size;

    if (info->found)
        return;

    if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
        return;

    vma = bfd_get_section_vma (abfd, section);
    size = bfd_get_section_size (section);

    if (0)
        OutputDebug("section: 0x%08" BFD_VMA_FMT "x - 0x%08" BFD_VMA_FMT "x (pc = 0x%08I64x)\n",
                vma, vma + size, info->pc);

    if (info->pc < vma)
        return;

    if (info->pc >= vma + size)
        return;

    info->found = bfd_find_nearest_line (abfd, section, module->syms, info->pc - vma,
                                         &info->filename, &info->functionname, &info->line);
}

#endif /* HAVE_BFD */


static BOOL
mgwhelp_find_symbol(HANDLE hProcess, DWORD64 Address, struct find_handle *info)
{
    DWORD64 Base;
    struct mgwhelp_process *process;
    struct mgwhelp_module *module;

    process = mgwhelp_process_lookup(hProcess);
    if (!process) {
        return FALSE;
    }

    Base = GetModuleBase64(hProcess, Address);
    if (!Base) {
        return FALSE;
    }

    module = mgwhelp_module_lookup(process, Base);
    if (!module)
        return FALSE;

    DWORD64 Offset = module->image_base_vma + Address - (DWORD64)module->Base;

    memset(info, 0, sizeof *info);
    info->module = module;
    info->pc = Offset;

    if (module->dbg) {
        find_dwarf_symbol(module, Offset, info);
        if (info->found) {
            return TRUE;
        }
    }

#if HAVE_BFD
    if (module->abfd) {
        assert(bfd_get_file_flags(module->abfd) & HAS_SYMS);
        assert(module->symcount);

        bfd_map_over_sections(module->abfd, find_address_in_section, info);
        if (info->found &&
            info->line != 0 &&
            info->functionname != NULL &&
            *info->functionname != '\0') {
            return TRUE;
        }
    }
#endif /* HAVE_BFD */

    return FALSE;
}


BOOL WINAPI
MgwSymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess)
{
    BOOL ret;

    ret = SymInitialize(hProcess, UserSearchPath, fInvadeProcess);

    if (ret) {
        struct mgwhelp_process *process;

        process = (struct mgwhelp_process *)calloc(1, sizeof *process);
        if (process) {
            process->hProcess = hProcess;

            process->next = processes;
            processes = process;
        }
    }

    return ret;
}

DWORD WINAPI
MgwSymSetOptions(DWORD SymOptions)
{
    return SymSetOptions(SymOptions);
}


BOOL WINAPI
MgwSymFromAddr(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol)
{
    struct find_handle info;

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
    struct find_handle info;

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
        char *res = demangle(DecoratedName);
        if (res) {
            strncpy(UnDecoratedName, res, UndecoratedLength);
            free(res);
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


