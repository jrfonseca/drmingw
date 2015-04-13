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


static char unknown[] = { '?', '?', '\0' };


static void
search_func(Dwarf_Debug dbg,
            Dwarf_Die die,
            Dwarf_Addr addr,
            char **rlt_func)
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

    do {

        if (*rlt_func != NULL)
            return;

        if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
            OutputDebug("MGWHELP: dwarf_tag failed - %s", dwarf_errmsg(de));
            goto cont_search;
        }

        if (tag == DW_TAG_subprogram) {
            if (dwarf_lowpc(die, &lopc, &de) != DW_DLV_OK ||
                dwarf_highpc_b(die, &hipc, &return_form, &return_class, &de) != DW_DLV_OK)
                goto cont_search;
            if (return_class == DW_FORM_CLASS_CONSTANT)
                hipc += lopc;
            if (addr < lopc || addr >= hipc)
                goto cont_search;

            /* Found it! */

            *rlt_func = unknown;
            ret = dwarf_attr(die, DW_AT_name, &sub_at, &de);
            if (ret == DW_DLV_ERROR)
                return;
            if (ret == DW_DLV_OK) {
                if (dwarf_formstring(sub_at, &func0, &de) != DW_DLV_OK)
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

        /* Recurse into children. */
        ret = dwarf_child(die, &child_die, &de);
        if (ret == DW_DLV_ERROR)
            OutputDebug("MGWHELP: dwarf_child failed - %s\n", dwarf_errmsg(de));
        else if (ret == DW_DLV_OK)
            search_func(dbg, child_die, addr, rlt_func);

        /* Advance to next sibling. */
        ret = dwarf_siblingof(dbg, die, &sibling_die, &de);
        if (ret != DW_DLV_OK) {
            if (ret == DW_DLV_ERROR)
                OutputDebug("MGWHELP: dwarf_siblingof failed - %s\n", dwarf_errmsg(de));
            break;
        }
        die = sibling_die;
    } while (true);
}


void
find_dwarf_symbol(Dwarf_Debug dbg,
                  Dwarf_Addr addr,
                  struct find_dwarf_info *info)
{
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
                OutputDebug("MGWHELP: dwarf_lineaddr failed - %s\n", dwarf_errmsg(error));
                break;
            }
            if (addr > plineaddr && addr < lineaddr) {
                lineno = plineno;
                file = pfile;
                break;
            }
            if (dwarf_lineno(linebuf[i], &lineno, &error) != DW_DLV_OK) {
                OutputDebug("MGWHELP: dwarf_lineno failed - %s\n", dwarf_errmsg(error));
                break;
            }
            if (dwarf_linesrc(linebuf[i], &file0, &error) != DW_DLV_OK) {
                OutputDebug("MGWHELP: dwarf_linesrc failed - %s\n", dwarf_errmsg(error));
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
        OutputDebug("MGWHELP: libdwarf error - %s\n", dwarf_errmsg(error));
    }
}
