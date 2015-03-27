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


void _ErrorMessageBox(LPCTSTR lpszFile, DWORD dwLine, LPCTSTR lpszFormat, ...)
{
    TCHAR szErrorMsg[1024], szModule[MAX_PATH], szMsg[4096];
    va_list ap;

    if(!GetModuleFileName(NULL, szModule, MAX_PATH))
        lstrcpy(szModule, _T(""));

    va_start(ap, lpszFormat);
    wvsprintf(szErrorMsg, lpszFormat, ap);
    va_end(ap);

    wsprintf(
        szMsg,
        _T(
            "Error!\r\n"
            "\r\n"
            "Program: %s\r\n"
            "File: %s\r\n"
            "Line: %i\r\n"
            "\r\n"
            "%s\r\n"
            "\r\n"
            "(Press Retry to debug the application - JIT must be enabled)\r\n"
        ),
        szModule,
        lpszFile,
        dwLine,
        szErrorMsg
    );

    // Display the string.
    switch (MessageBox(NULL, szMsg, _T("DrMingw"), MB_ICONERROR | MB_ABORTRETRYIGNORE))
    {
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


