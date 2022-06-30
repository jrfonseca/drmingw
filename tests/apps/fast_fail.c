/**************************************************************************
 *
 * Copyright 2015 Jose Fonseca
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

// http://www.alex-ionescu.com/?p=69

#include <windows.h>

#include "macros.h"
#define PF_FASTFAIL_AVAILABLE 23

BOOL WINAPI IsProcessorFeaturePresent(DWORD ProcessorFeature);

#define FAST_FAIL_FATAL_APP_EXIT 7

#if defined(__MINGW64__) && defined(_M_ARM64)
static NO_RETURN FORCE_INLINE
void __fastfail(unsigned int code) {
    register unsigned int w0 __asm__ ("w0") = code;
    asm volatile (
        "brk #0xF003"
         :
         : "r" (w0)
     );
    __builtin_unreachable();
}

#elif defined(__MINGW32__)

static NO_RETURN FORCE_INLINE
void __fastfail(unsigned int code) {
    asm volatile (
        "int $0x29"
         :
         : "c" (code)
     );
    __builtin_unreachable();
}

#endif

int
main(int argc, char *argv[])
{
    if (!IsProcessorFeaturePresent(PF_FASTFAIL_AVAILABLE)) {
        return 3;
    }

    __fastfail(FAST_FAIL_FATAL_APP_EXIT);  LINE_BARRIER

    return 0;
}
