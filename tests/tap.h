/**************************************************************************
 *
 * Copyright 2015 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


/*
 * https://testanything.org/tap-specification.html
 */

#pragma once


#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif


static int
test_status = EXIT_SUCCESS;


static unsigned
test_no = 0;


__attribute__ ((format (printf, 2, 3)))
static void
test_line(bool ok, const char *format, ...)
{
    va_list ap;

    if (!ok) {
        test_status = EXIT_FAILURE;
    }

    fprintf(stdout, "%s %u",
            ok ? "ok" : "not ok",
            ++test_no);
    if (format) {
        fputs(" - ", stdout);
        va_start(ap, format);
        vfprintf(stdout, format, ap);
        va_end(ap);
    }
    fputs("\n", stdout);
    fflush(stdout);
}


__attribute__ ((format (printf, 1, 2)))
static void
test_diagnostic(const char *format, ...)
{
    va_list ap;

    fputs("# ", stdout);
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
    fputs("\n", stdout);
    fflush(stdout);
}


#ifdef _WIN32

static inline void
test_diagnostic_last_error(void)
{
    DWORD dwLastError = GetLastError();

    // http://msdn.microsoft.com/en-gb/library/windows/desktop/ms680582.aspx
    LPSTR lpErrorMsg = NULL;
    DWORD cbWritten;
    cbWritten = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL,
                               dwLastError,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (LPSTR) &lpErrorMsg,
                               0, NULL);

    if (cbWritten) {
        test_diagnostic("%s", lpErrorMsg);
    } else {
        test_diagnostic("GetLastError() = %lu", dwLastError);
    }

    LocalFree(lpErrorMsg);
}

#endif


__attribute__((__noreturn__))
static void
test_exit(void)
{
    fprintf(stdout, "1..%u\n", test_no);
    fflush(stdout);
    exit(test_status);
}
