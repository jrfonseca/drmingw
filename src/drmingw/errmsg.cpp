/*
 * Copyright 2002-2013 Jose Fonseca
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

#include "errmsg.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>


void
_ErrorMessageBox(LPCWSTR lpszFile, DWORD dwLine, LPCWSTR lpszFormat, ...)
{
    wchar_t szErrorMsg[1024], szMsg[4096];
    wchar_t szModule[MAX_PATH];
    va_list ap;

    if (!GetModuleFileNameW(NULL, szModule, MAX_PATH)) {
        szModule[0] = 0;
    }

    va_start(ap, lpszFormat);
    vswprintf(szErrorMsg, lpszFormat, ap);
    va_end(ap);

    swprintf(szMsg,
            L"Error!\r\n"
            L"\r\n"
            L"Program: %ls\r\n"
            L"File: %ls\r\n"
            L"Line: %lu\r\n"
            L"\r\n"
            L"%ls\r\n"
            L"\r\n"
            L"(Press Retry to debug the application - JIT must be enabled)\r\n",
            szModule, lpszFile, dwLine, szErrorMsg);

    // Display the string.
    switch (MessageBoxW(NULL, szMsg, L"DrMingw", MB_ICONERROR | MB_ABORTRETRYIGNORE)) {
    case IDABORT:
        _exit(3);
        return;

    case IDRETRY:
        DebugBreak();
        return;

    case IDIGNORE:
        return;
    }
}
