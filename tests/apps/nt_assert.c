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

#include <windows.h>

#include "macros.h"

#if defined(__MINGW64__) && defined(_M_ARM64)

static NO_RETURN FORCE_INLINE
void DbgRaiseAssertionFailure(void) {
    asm volatile (
        "brk #0xf001"
    );
    __builtin_unreachable();
}

#elif defined(__MINGW32__)

#undef DbgRaiseAssertionFailure

static NO_RETURN FORCE_INLINE
void DbgRaiseAssertionFailure(void) {
    asm volatile (
        "int $0x2c"
    );
    __builtin_unreachable();
}

#endif

#include "macros.h"

int
main(int argc, char *argv[])
{
    DbgRaiseAssertionFailure();  LINE_BARRIER

    return 0;
}
