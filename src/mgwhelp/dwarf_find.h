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

#include <stdbool.h>

#include <dwarf.h>
#include <libdwarf.h>


#ifdef __cplusplus
extern "C" {
#endif


struct find_dwarf_info
{
    const char *filename;
    const char *functionname;
    unsigned int line;
    bool found;
};


void
find_dwarf_symbol(Dwarf_Debug dbg,
                  Dwarf_Addr addr,
                  struct find_dwarf_info *info);

#ifdef __cplusplus
}
#endif
