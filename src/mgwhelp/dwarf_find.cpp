/*
 * Copyright 2013-2015 Jose Fonseca
 * Copyright 2009 Kai Wang
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
 *
 *
 * Based on elftoolchain-0.6.1/addr2line/addr2line.c
 */


#include "dwarf_find.h"

#include <stdlib.h>

#include "outdbg.h"


static char unknown[] = {'?', '?', '\0'};


static bool
search_func(Dwarf_Debug dbg,
            Dwarf_Die *die,
            Dwarf_Addr addr,
            unsigned int *offset_addr,
            std::string &rlt_func)
{
    Dwarf_Die spec_die;
    Dwarf_Die child_die;
    Dwarf_Die sibling_die;
    Dwarf_Error de;
    Dwarf_Half tag, return_form;
    Dwarf_Unsigned lopc, hipc;
    Dwarf_Off ref;
    Dwarf_Attribute sub_at, spec_at;
    char *func0;
    int ret;
    enum Dwarf_Form_Class return_class;
    bool result;

    do {
        if (dwarf_tag(*die, &tag, &de) != DW_DLV_OK) {
            OutputDebug("MGWHELP: dwarf_tag failed - %s", dwarf_errmsg(de));
            goto cont_search;
        }

        if (tag == DW_TAG_subprogram) {
            if (dwarf_lowpc(*die, &lopc, &de) != DW_DLV_OK ||
                dwarf_highpc_b(*die, &hipc, &return_form, &return_class, &de) != DW_DLV_OK)
                goto cont_search;
            if (return_class == DW_FORM_CLASS_CONSTANT)
                hipc += lopc;
            if (addr < lopc || addr >= hipc)
                goto cont_search;

            /* Found it! */

            *offset_addr = addr - lopc;
            rlt_func = unknown;
            ret = dwarf_attr(*die, DW_AT_name, &sub_at, &de);
            if (ret == DW_DLV_ERROR)
                return false;
            if (ret == DW_DLV_OK) {
                if (dwarf_formstring(sub_at, &func0, &de) == DW_DLV_OK)
                    rlt_func = func0;
                dwarf_dealloc(dbg, sub_at, DW_DLA_ATTR);
                return true;
            }

            ret = dwarf_attr(*die, DW_AT_linkage_name, &sub_at, &de);
            if (ret == DW_DLV_ERROR)
                return false;
            if (ret == DW_DLV_OK) {
                if (dwarf_formstring(sub_at, &func0, &de) == DW_DLV_OK)
                    rlt_func = func0;
                dwarf_dealloc(dbg, sub_at, DW_DLA_ATTR);
                return true;
            }

            ret = dwarf_attr(*die, DW_AT_MIPS_linkage_name, &sub_at, &de);
            if (ret == DW_DLV_ERROR)
                return false;
            if (ret == DW_DLV_OK) {
                if (dwarf_formstring(sub_at, &func0, &de) == DW_DLV_OK)
                    rlt_func = func0;
                dwarf_dealloc(dbg, sub_at, DW_DLA_ATTR);
                return true;
            }

            /*
             * If DW_AT_name is not present, but DW_AT_specification is
             * present, then probably the actual name is in the DIE
             * referenced by DW_AT_specification.
             */
            if (dwarf_attr(*die, DW_AT_specification, &spec_at, &de) != DW_DLV_OK)
                return false;
            if (dwarf_global_formref(spec_at, &ref, &de) != DW_DLV_OK) {
                dwarf_dealloc(dbg, spec_at, DW_DLA_ATTR);
                return false;
            }
            if (dwarf_offdie(dbg, ref, &spec_die, &de) != DW_DLV_OK) {
                dwarf_dealloc(dbg, spec_at, DW_DLA_ATTR);
                return false;
            }
            if (dwarf_diename(spec_die, &func0, &de) == DW_DLV_OK)
                rlt_func = func0;
            dwarf_dealloc(dbg, spec_die, DW_DLA_DIE);
            dwarf_dealloc(dbg, spec_at, DW_DLA_ATTR);
            return true;
        }

cont_search:

        /* Recurse into children. */
        ret = dwarf_child(*die, &child_die, &de);
        if (ret == DW_DLV_ERROR)
            OutputDebug("MGWHELP: dwarf_child failed - %s\n", dwarf_errmsg(de));
        else if (ret == DW_DLV_OK) {
            result = search_func(dbg, &child_die, addr, offset_addr, rlt_func);
            dwarf_dealloc(dbg, child_die, DW_DLA_DIE);
            if (result) {
                return true;
            }
        }

        /* Advance to next sibling. */
        ret = dwarf_siblingof(dbg, *die, &sibling_die, &de);
        if (ret != DW_DLV_OK) {
            if (ret == DW_DLV_ERROR)
                OutputDebug("MGWHELP: dwarf_siblingof failed - %s\n", dwarf_errmsg(de));
            return false;
        }
        dwarf_dealloc(dbg, *die, DW_DLA_DIE);
        *die = sibling_die;
    } while (true);
}


bool
dwarf_find_symbol(dwarf_module *dwarf, Dwarf_Addr addr, struct dwarf_symbol_info *info)
{
    bool result = false;
    Dwarf_Error error = 0;

    Dwarf_Debug dbg = dwarf->dbg;

    Dwarf_Arange arange;
    if (dwarf_get_arange(dwarf->aranges, dwarf->arange_count, addr, &arange, &error) != DW_DLV_OK) {
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

    result = search_func(dbg, &cu_die, addr, &info->offset_addr, info->functionname);

    dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
no_cu_die:;
no_die_offset:;
no_arange:
    if (error) {
        OutputDebug("MGWHELP: libdwarf error - %s\n", dwarf_errmsg(error));
    }
    return result;
}

bool
dwarf_find_line(dwarf_module *dwarf, Dwarf_Addr addr, struct dwarf_line_info *info)
{
    bool result = false;
    Dwarf_Error error = 0;
    std::string symbol_name;
    unsigned int offset_addr;

    Dwarf_Debug dbg = dwarf->dbg;

    Dwarf_Arange arange;
    if (dwarf_get_arange(dwarf->aranges, dwarf->arange_count, addr, &arange, &error) != DW_DLV_OK) {
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

    Dwarf_Line *linebuf;
    Dwarf_Signed linecount;
    if (search_func(dbg, &cu_die, addr, &offset_addr, symbol_name) &&
        dwarf_srclines(cu_die, &linebuf, &linecount, &error) == DW_DLV_OK) {
        Dwarf_Unsigned lineno, plineno;
        Dwarf_Addr lineaddr, plineaddr;
        char *file, *pfile;
        plineaddr = ~0ULL;
        plineno = lineno = 0;
        pfile = file = nullptr;
        Dwarf_Signed i;

        i = 0;
        while (i < linecount) {
            if (dwarf_lineaddr(linebuf[i], &lineaddr, &error) != DW_DLV_OK) {
                OutputDebug("MGWHELP: dwarf_lineaddr failed - %s\n", dwarf_errmsg(error));
                break;
            }

            if (lineaddr == 0) {
                /* Per dwarfdump/print_lines.c, The SN Systems Linker generates
                 * line records with addr=0, when dealing with linkonce symbols
                 * and no stripping.  We need to skip records that do not have
                 * ís_addr_set.
                 */
                ++i;
                while (i < linecount) {
                    Dwarf_Bool has_is_addr_set = FALSE;
                    if (dwarf_line_is_addr_set(linebuf[i], &has_is_addr_set, &error) != DW_DLV_OK) {
                        OutputDebug("MGWHELP: dwarf_line_is_addr_set failed - %s\n",
                                    dwarf_errmsg(error));
                        has_is_addr_set = FALSE;
                    }
                    if (has_is_addr_set) {
                        break;
                    }
                    ++i;
                }
                continue;
            }

            if (addr > plineaddr && addr < lineaddr) {
                // Lines are past the address
                lineno = plineno;
                file = pfile;
                pfile = nullptr;
                result = true;
                break;
            }

            if (dwarf_lineno(linebuf[i], &lineno, &error) != DW_DLV_OK) {
                OutputDebug("MGWHELP: dwarf_lineno failed - %s\n", dwarf_errmsg(error));
                break;
            }

            if (dwarf_linesrc(linebuf[i], &file, &error) != DW_DLV_OK) {
                OutputDebug("MGWHELP: dwarf_linesrc failed - %s\n", dwarf_errmsg(error));
            }

            if (addr == lineaddr) {
                // Exact match
                result = true;
                break;
            }

            plineaddr = lineaddr;
            plineno = lineno;
            if (pfile) {
                dwarf_dealloc(dbg, pfile, DW_DLA_STRING);
            }
            pfile = file;
            file = NULL;
            ++i;
        }

        if (result && file) {
            info->filename = file;
            info->line = lineno;
            info->offset_addr = offset_addr;
        }
        if (file) {
            dwarf_dealloc(dbg, file, DW_DLA_STRING);
        }
        if (pfile) {
            dwarf_dealloc(dbg, pfile, DW_DLA_STRING);
        }

        dwarf_srclines_dealloc(dbg, linebuf, linecount);
    }

    dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
no_cu_die:;
no_die_offset:;
no_arange:
    if (error) {
        OutputDebug("MGWHELP: libdwarf error - %s\n", dwarf_errmsg(error));
    }
    return result;
}
