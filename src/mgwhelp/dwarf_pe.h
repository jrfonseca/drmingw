/*
 * Copyright 2012 Jose Fonseca
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

#include <windows.h>

#include <dwarf.h>
#include <libdwarf.h>


#ifdef __cplusplus
extern "C" {
#endif


int
dwarf_pe_init(const char *image,
              Dwarf_Handler errhand,
              Dwarf_Ptr errarg,
              Dwarf_Debug * ret_dbg, Dwarf_Error * error);

BOOL
dwarf_pe_find_symbol(Dwarf_Debug dbg,
                     DWORD64 Addr,
                     ULONG MaxSymbolNameLen,
                     LPSTR pSymbolName,
                     PDWORD64 pDisplacement);

int
dwarf_pe_finish(Dwarf_Debug dbg, Dwarf_Error * error);


#ifdef __cplusplus
}
#endif
