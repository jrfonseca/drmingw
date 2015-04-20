/**************************************************************************
 *
 * Copyright 2009 Jose Fonseca
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

#ifdef NDEBUG
#error "NDEBUG defined"
#endif

#include <stdlib.h>
#include <assert.h>

int main()
{
    // Disable assertion failure message box
    // http://msdn.microsoft.com/en-us/library/sas1dkb2.aspx
    _set_error_mode(_OUT_TO_STDERR);
#ifdef _MSC_VER
    // Disable abort message box
    // http://msdn.microsoft.com/en-us/library/e631wekh.aspx
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    assert(2 + 2 == 5);
    return 0;
}

// CHECK_STDERR: /2 \+ 2 == 5/
// CHECK_EXIT_CODE: 3
