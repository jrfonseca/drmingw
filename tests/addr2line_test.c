/*
 * Copyright 2015 Jose Fonseca
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


#include "tap.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <windows.h>


static void
Foo(void)
{
}


int
main(int argc, char **argv)
{
    const char *szSymbolName = "Foo";
    bool ok;

    HMODULE hModule = GetModuleHandleA(NULL);
    assert(hModule != NULL);

    DWORD64 dwSymbolOffset = (DWORD64)(UINT_PTR)&Foo - (DWORD64)(UINT_PTR)hModule;

    char szCommand[1024];
    _snprintf(szCommand, sizeof szCommand, "addr2line.exe -e %s -f 0x%I64x", argv[0], dwSymbolOffset);

    FILE *fp = _popen(szCommand, "rt");
    ok = fp != NULL;
    test_line(ok, "_popen(\"%s\")", szCommand);
    if (ok) {
        char szLine[512];
        bool found = false;

        while (fgets(szLine, sizeof szLine, fp)) {
            fprintf(stdout, "%s\n", szLine);
            if (strstr(szLine, szSymbolName)) {
                found = true;
            }
        }

        test_line(found, "strstr(\"%s\")", szSymbolName);

        fclose(fp);
    }

    test_exit();
}
