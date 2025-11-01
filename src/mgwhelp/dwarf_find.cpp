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
#include <stdlib.h>

#include "libdwarf.h"
#include <dwarf_arange.h>
#include <dwarfstack.h>

static void
find_symbol_cb(Dwarf_Addr addr,
               const char *filename,
               int lineno,
               const char *funcname,
               void *context,
               int columnno)
{
    switch (lineno) {
    case DWST_BASE_ADDR:
    case DWST_NOT_FOUND:
        break;

    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
        break;

    default:
        auto info = (struct dwarf_symbol_info *)context;
        info->functionname = funcname;
        info->offset_addr = addr;
        return;
    }
}

bool
dwarf_find_symbol(Dwarf_Debug dbg,
                  void *cuArr,
                  int cuQty,
                  Dwarf_Addr image_base_vma,
                  char *name,
                  Dwarf_Addr image_base,
                  Dwarf_Addr addr,
                  struct dwarf_symbol_info *info)
{
    dwstOfDwarfDebug(dbg, image_base_vma, name, image_base, &addr, 1, &find_symbol_cb, info, cuArr,
                     cuQty);
    return !info->functionname.empty();
}

static void
find_line_cb(Dwarf_Addr addr,
             const char *filename,
             int lineno,
             const char *funcname,
             void *context,
             int columnno)
{
    switch (lineno) {
    case DWST_BASE_ADDR:
    case DWST_NOT_FOUND:
        break;

    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
        break;

    default:
        auto info = (struct dwarf_line_info *)context;
        info->filename = filename;
        info->offset_addr = addr;
        info->line = lineno;
        return;
    }
}

bool
dwarf_find_line(Dwarf_Debug dbg,
                void *cuArr,
                int cuQty,
                Dwarf_Addr image_base_vma,
                char *name,
                Dwarf_Addr image_base,
                Dwarf_Addr addr,
                struct dwarf_line_info *info)
{
    dwstOfDwarfDebug(dbg, image_base_vma, name, image_base, &addr, 1, &find_line_cb, info, cuArr,
                     cuQty);

    return !info->filename.empty();
}
