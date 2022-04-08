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

#include <stdio.h>
#include <limits.h>

#include "wine.h"


static double factorial(int depth, void *inBuf)
{
    char buf[1024];
    if (depth) {
        return depth * factorial(depth - 1, buf);
    } else {
        return 1;
    }
}


static LONG CALLBACK
unhandledExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;
    DWORD ExceptionCode = pExceptionRecord->ExceptionCode;
    if (ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        TerminateProcess(GetCurrentProcess(), EXCEPTION_STACK_OVERFLOW);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


int main()
{
    // Wine exits with status 0, and without giving the debugger 2nd chance.
    if (insideWine()) {
#ifdef _WIN64
        // unhandledExceptionHandler never gets called
        return EXIT_SKIP;
#endif
        AddVectoredExceptionHandler(0, unhandledExceptionHandler);
    }

    int x = INT_MAX;
    printf("factorial(%i) = %g\n", x, factorial(x, NULL));

    return 0;
}

// CHECK_STDERR: /  stack_overflow\.exe\!factorial\+0x[0-9a-f]+  \[.*\bstack_overflow\.c @ 38\]/
// CHECK_STDERR: /  stack_overflow\.exe\!main\+0x[0-9a-f]+  \[.*\bstack_overflow\.c @ 69\]/
// CHECK_EXIT_CODE: 0xc00000fd
