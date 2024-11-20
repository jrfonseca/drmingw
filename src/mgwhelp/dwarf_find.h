/*
 * Copyright 2013-2015 Jose Fonseca
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


#pragma once

#include <string>

#include <stdbool.h>

#include <dwarf.h>
#include <libdwarf.h>
#include <vector>


#ifdef __cplusplus
extern "C" {
#endif


struct dwarf_symbol_info {
    std::string functionname;
    unsigned int offset_addr;
};

struct dwarf_line_info {
    std::string filename;
    unsigned int line = 0;
    unsigned int offset_addr;
};

typedef struct _My_Arange {
    /*  The segment selector. Only non-zero if Dwarf4, only
        meaningful if ar_segment_selector_size non-zero   */
    Dwarf_Unsigned ar_segment_selector;

    /* Starting address of the arange, ie low-pc. */
    Dwarf_Addr ar_address;

    /* Length of the arange. */
    Dwarf_Unsigned ar_length;

    /*  Offset into .debug_info of the start of the compilation-unit
        containing this set of aranges.
        Applies only to .debug_info, not .debug_types. */
    Dwarf_Off ar_info_offset;

    /* Corresponding Dwarf_Debug. */
    Dwarf_Debug ar_dbg;

    Dwarf_Half ar_segment_selector_size;
} My_Arange;

struct dwarf_module {
    Dwarf_Debug dbg;

    // cached aranges
    Dwarf_Arange *aranges;
    Dwarf_Signed arange_count;

    std::vector<My_Arange *> my_aranges;
};


bool
dwarf_find_symbol(dwarf_module *dwarf, Dwarf_Addr addr, struct dwarf_symbol_info *info);

bool
dwarf_find_line(dwarf_module *dwarf, Dwarf_Addr addr, struct dwarf_line_info *info);

#ifdef __cplusplus
}
#endif
