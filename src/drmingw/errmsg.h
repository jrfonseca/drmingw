/*
 * Copyright 2002-2015 Jose Fonseca
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

#pragma once


#include <windows.h>


// XXX: This leaks the error message
#define FormatErrorMessage(n) \
    ({ \
        LPVOID lpMsgBuf; \
\
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | \
                           FORMAT_MESSAGE_IGNORE_INSERTS, \
                       NULL, n, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, \
                       NULL); \
\
        (LPWSTR) lpMsgBuf; \
    })

#define LastErrorMessage() FormatErrorMessage(GetLastError())

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFILE WIDE1(__FILE__)

void
_ErrorMessageBox(LPCWSTR lpszFile, DWORD dwLine, LPCWSTR lpszFormat, ...);
#define ErrorMessageBox(e, args...) _ErrorMessageBox(WFILE, __LINE__, e, ##args)
