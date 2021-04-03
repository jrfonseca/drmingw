/**************************************************************************
 *
 * Copyright 2016 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OF OR CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 **************************************************************************/

#pragma once

#include <windows.h>
#include <stdio.h>

typedef const char * (CDECL *WINE_GET_VERSION)(void);

// http://wiki.winehq.org/DeveloperFaq#detect-wine
static __inline BOOL
insideWine(void)
{
    HMODULE hNtDll = GetModuleHandleA("ntdll");
    if (!hNtDll) {
        return FALSE;
    }
    return GetProcAddress(hNtDll, "wine_get_version") != NULL;
}

static __inline BOOL
insideWineOlderThan(unsigned refMajor, unsigned refMinor)
{
    HMODULE hNtDll = GetModuleHandleA("ntdll");
    if (hNtDll) {
        WINE_GET_VERSION wine_get_version;
        wine_get_version = (WINE_GET_VERSION)GetProcAddress(hNtDll, "wine_get_version");
        if (wine_get_version) {
            const char *wine_version = wine_get_version();
            unsigned major = 0, minor = 0;
            if (sscanf(wine_version, "%u.%u", &major, &minor) == 2 &&
                (major < refMajor ||
                 (major == refMajor && minor < refMinor))) {
                return TRUE;
            }
        }
    }
    return FALSE;
}
