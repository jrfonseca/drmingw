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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <vector>

#include "dwarf.h"
#include "libdwarf.h"
#include "outdbg.h"

static int
create_aranges(Dwarf_Debug dbg, std::vector<My_Arange *> &myrec, Dwarf_Error *error);

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
            if (dwarf_offdie_b(dbg, ref, 1, &spec_die, &de) != DW_DLV_OK) {
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
        ret = dwarf_siblingof_b(dbg, *die, 1, &sibling_die, &de);
        if (ret != DW_DLV_OK) {
            if (ret == DW_DLV_ERROR)
                OutputDebug("MGWHELP: dwarf_siblingof_b failed - %s\n", dwarf_errmsg(de));
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
        if (dwarf->my_aranges.empty()) {
            if (create_aranges(dbg, dwarf->my_aranges, &error) == DW_DLV_ERROR) {
                goto no_arange;
            }
        }
        if (dwarf_get_arange((Dwarf_Arange *)dwarf->my_aranges.data(), dwarf->my_aranges.size(),
                             addr, &arange, &error) != DW_DLV_OK) {
            goto no_arange;
        }
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
        if (dwarf->my_aranges.empty()) {
            if (create_aranges(dbg, dwarf->my_aranges, &error) == DW_DLV_ERROR) {
                goto no_arange;
            }
        }
        if (dwarf_get_arange((Dwarf_Arange *)dwarf->my_aranges.data(), dwarf->my_aranges.size(),
                             addr, &arange, &error) != DW_DLV_OK) {
            goto no_arange;
        }
    }

    Dwarf_Off cu_die_offset;
    if (dwarf_get_cu_die_offset(arange, &cu_die_offset, &error) != DW_DLV_OK) {
        goto no_die_offset;
    }

    Dwarf_Die cu_die;
    if (dwarf_offdie_b(dbg, cu_die_offset, 1, &cu_die, &error) != DW_DLV_OK) {
        goto no_cu_die;
    }

    if (!search_func(dbg, &cu_die, addr, &offset_addr, symbol_name)) {
        goto no_func;
    }

    Dwarf_Unsigned version;
    Dwarf_Small table_count;
    Dwarf_Line_Context context;
    version = 0;
    table_count = 0;
    context = 0;
    if (dwarf_srclines_b(cu_die, &version, &table_count, &context, &error) != DW_DLV_OK) {
        goto no_linecontext;
    }

    Dwarf_Line *linebuf;
    Dwarf_Signed linecount;
    if (dwarf_srclines_from_linecontext(context, &linebuf, &linecount, &error) != DW_DLV_OK) {
        goto no_srclines;
    }

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

no_srclines:;
    dwarf_srclines_dealloc_b(context);
no_linecontext:;
no_func:
    dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
no_cu_die:;
no_die_offset:;
no_arange:
    if (error) {
        OutputDebug("MGWHELP: libdwarf error - %s\n", dwarf_errmsg(error));
    }
    return result;
}

static int
getlowhighpc(Dwarf_Die die, Dwarf_Addr *lowpc_out, Dwarf_Addr *highpc_out, Dwarf_Error *error)
{
    Dwarf_Addr hipc = 0;
    int res = 0;
    Dwarf_Half form = 0;
    enum Dwarf_Form_Class formclass = DW_FORM_CLASS_UNKNOWN;

    res = dwarf_lowpc(die, lowpc_out, error);
    if (res == DW_DLV_OK) {
        res = dwarf_highpc_b(die, &hipc, &form, &formclass, error);
        if (res == DW_DLV_OK) {
            if (formclass == DW_FORM_CLASS_CONSTANT) {
                hipc += *lowpc_out;
            }
            *highpc_out = hipc;
            return DW_DLV_OK;
        }
    }
    /*  Cannot check ranges yet, we don't know the ranges base
        offset yet. */
    return DW_DLV_NO_ENTRY;
}

/* Based on 'examplev' in 'checkexamples.c'.
   There is another example in 'check_comp_dir()' in 'findfuncbypc.c',
   but it seems wrong because the base address is not read from 'DW_AT_low_pc' of the CU die.
 */
static int
record_range(Dwarf_Debug dbg,
             Dwarf_Die die,
             std::vector<My_Arange *> &myrec,
             Dwarf_Off info_offset,
             Dwarf_Error *error)
{
    Dwarf_Signed count = 0;
    Dwarf_Off realoffset = 0;
    Dwarf_Ranges *rangesbuf = 0;
    Dwarf_Unsigned bytecount = 0;
    int res = 0;
    Dwarf_Unsigned base_address = 0;
    Dwarf_Bool have_base_addr = FALSE;
    Dwarf_Bool have_rangesoffset = FALSE;
    Dwarf_Unsigned rangesoffset = (Dwarf_Unsigned)info_offset;

    /*  Find the ranges for a specific DIE */
    res = dwarf_get_ranges_baseaddress(dbg, die, &have_base_addr, &base_address, &have_rangesoffset,
                                       &rangesoffset, error);
    if (res == DW_DLV_ERROR) {
        /* Just pretend not an error. */
        dwarf_dealloc_error(dbg, *error);
        *error = 0;
    }

    res = dwarf_get_ranges_b(dbg, rangesoffset, die, &realoffset, &rangesbuf, &count, &bytecount,
                             error);
    if (res != DW_DLV_OK) {
        return res;
    }
    Dwarf_Signed i = 0;
    for (i = 0; i < count; ++i) {
        Dwarf_Ranges *cur = rangesbuf + i;
        Dwarf_Addr base = base_address;
        Dwarf_Addr lowpc;
        Dwarf_Addr highpc;

        switch (cur->dwr_type) {
        case DW_RANGES_ENTRY:
            lowpc = cur->dwr_addr1 + base;
            highpc = cur->dwr_addr2 + base;
            myrec.push_back(new My_Arange{0, lowpc, highpc - lowpc, info_offset, dbg, 0});
            break;
        case DW_RANGES_ADDRESS_SELECTION:
            base = cur->dwr_addr2;
            break;
        case DW_RANGES_END:
            break;
        default:
            fprintf(stderr,
                    "Impossible debug_ranges content!"
                    " enum val %d \n",
                    (int)cur->dwr_type);
            return DW_DLV_ERROR;
        }
    }
    dwarf_dealloc_ranges(dbg, rangesbuf, count);

    return DW_DLV_OK;
}

/* Based on 'example_rnglist_for_attribute()' in 'checkexamples.c'. */
static int
record_rnglist_for_attribute(Dwarf_Debug dbg,
                             Dwarf_Attribute attr,
                             Dwarf_Unsigned attrvalue,
                             Dwarf_Half form,
                             std::vector<My_Arange *> &myrec,
                             Dwarf_Off info_offset,
                             Dwarf_Error *error)
{
    /*  attrvalue must be the DW_AT_ranges
        DW_FORM_rnglistx or DW_FORM_sec_offset value
        extracted from attr. */
    int res = 0;
    Dwarf_Unsigned entries_count;
    Dwarf_Unsigned global_offset_of_rle_set;
    Dwarf_Rnglists_Head rnglhead = 0;
    Dwarf_Unsigned i = 0;

    res = dwarf_rnglists_get_rle_head(attr, form, attrvalue, &rnglhead, &entries_count,
                                      &global_offset_of_rle_set, error);
    if (res != DW_DLV_OK) {
        return res;
    }
    for (i = 0; i < entries_count; ++i) {
        unsigned entrylen = 0;
        unsigned code = 0;
        Dwarf_Unsigned rawlowpc = 0;
        Dwarf_Unsigned rawhighpc = 0;
        Dwarf_Bool debug_addr_unavailable = FALSE;
        Dwarf_Unsigned lowpc = 0;
        Dwarf_Unsigned highpc = 0;

        /*  Actual addresses are most likely what one
            wants to know, not the lengths/offsets
            recorded in .debug_rnglists. */
        res =
            dwarf_get_rnglists_entry_fields_a(rnglhead, i, &entrylen, &code, &rawlowpc, &rawhighpc,
                                              &debug_addr_unavailable, &lowpc, &highpc, error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_rnglists_head(rnglhead);
            return res;
        }
        if (code == DW_RLE_end_of_list) {
            /* we are done */
            break;
        }
        if (code == DW_RLE_base_addressx || code == DW_RLE_base_address) {
            /*  We do not need to use these, they
                have been accounted for already. */
            continue;
        }
        if (debug_addr_unavailable) {
            /* lowpc and highpc are not real addresses */
            continue;
        }
        /*  Here do something with lowpc and highpc, these
            are real addresses */
        myrec.push_back(new My_Arange{0, lowpc, highpc - lowpc, info_offset, dbg, 0});
    }
    dwarf_dealloc_rnglists_head(rnglhead);
    return DW_DLV_OK;
}

/* Based on 'print_die_data_i() in 'simplereader.c'. */
static int
record_rnglist(Dwarf_Debug dbg,
               Dwarf_Die cur_die,
               std::vector<My_Arange *> &myrec,
               Dwarf_Off info_offset,
               Dwarf_Error *error)
{
    Dwarf_Attribute attr = nullptr;
    int res = DW_DLV_OK;
    Dwarf_Unsigned attrvalue = 0;
    Dwarf_Half form = 0;

    res = dwarf_attr(cur_die, DW_AT_ranges, &attr, error);
    if (res != DW_DLV_OK)
        return res;

    res = dwarf_whatform(attr, &form, error);
    if (res == DW_DLV_OK) {
        if (form == DW_FORM_rnglistx) {
            res = dwarf_formudata(attr, &attrvalue, error);
        } else {
            assert(form == DW_FORM_sec_offset);
            res = dwarf_global_formref(attr, &attrvalue, error);
        }
        if (res == DW_DLV_OK) {
            res =
                record_rnglist_for_attribute(dbg, attr, attrvalue, form, myrec, info_offset, error);
        }
    }
    dwarf_dealloc(dbg, attr, DW_DLA_ATTR);

    return res;
}

static int
record_die(Dwarf_Debug dbg,
           Dwarf_Die cur_die,
           int is_info,
           int in_level,
           std::vector<My_Arange *> &myrec,
           Dwarf_Error *error)
{
    Dwarf_Off info_offset, cu_offset;
    Dwarf_Addr lowpc = 0;
    Dwarf_Addr highpc = 0;
    int res;

    res = dwarf_dieoffset(cur_die, &info_offset, error);
    if (res != DW_DLV_OK)
        return res;
    dwarf_die_CU_offset(cur_die, &cu_offset, error);
    if (res != DW_DLV_OK)
        return res;
    info_offset -= cu_offset;

    res = getlowhighpc(cur_die, &lowpc, &highpc, error);
    if (res == DW_DLV_OK) {
        myrec.push_back(new My_Arange{0, lowpc, highpc - lowpc, info_offset, dbg, 0});
    } else {
        Dwarf_Attribute attr = 0;
        Dwarf_Half version = 0;
        Dwarf_Half offset_size = 0;
        res = dwarf_attr(cur_die, DW_AT_ranges, &attr, error);
        if (res != DW_DLV_OK)
            return res;
        res = dwarf_get_version_of_die(cur_die, &version, &offset_size);
        if (res != DW_DLV_OK)
            return res;

        if (version <= 4) {
            res = record_range(dbg, cur_die, myrec, info_offset, error);
        } else {
            res = record_rnglist(dbg, cur_die, myrec, info_offset, error);
        }
        if (res != DW_DLV_OK)
            return res;
    }
    return DW_DLV_OK;
}

int
create_aranges(Dwarf_Debug dbg, std::vector<My_Arange *> &myrec, Dwarf_Error *error)
{
    Dwarf_Unsigned abbrev_offset = 0;
    Dwarf_Half address_size = 0;
    Dwarf_Half version_stamp = 0;
    Dwarf_Half offset_size = 0;
    Dwarf_Half extension_size = 0;
    Dwarf_Sig8 signature;
    Dwarf_Unsigned typeoffset = 0;
    Dwarf_Unsigned next_cu_header = 0;
    Dwarf_Half header_cu_type = 0;
    Dwarf_Bool is_info = TRUE;
    int res = DW_DLV_OK;

    while (true) {
        Dwarf_Die cu_die = NULL;
        Dwarf_Unsigned cu_header_length = 0;

        memset(&signature, 0, sizeof(signature));
        res = dwarf_next_cu_header_e(dbg, is_info, &cu_die, &cu_header_length, &version_stamp,
                                     &abbrev_offset, &address_size, &offset_size, &extension_size,
                                     &signature, &typeoffset, &next_cu_header, &header_cu_type,
                                     error);
        if (res == DW_DLV_ERROR) {
            return res;
        }
        if (res == DW_DLV_NO_ENTRY) {
            if (is_info == TRUE) {
                /*  Done with .debug_info, now check for
                    .debug_types. */
                is_info = FALSE;
                continue;
            }
            /*  No more CUs to read! Never found
                what we were looking for in either
                .debug_info or .debug_types. */
            return res;
        }
        /*  We have the cu_die . */
        res = record_die(dbg, cu_die, is_info, 0, myrec, error);
        dwarf_dealloc_die(cu_die);
    }
    return res;
}
