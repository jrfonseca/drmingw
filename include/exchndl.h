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


// Set the unhandled exception handler.
// Must be called when exchndll.dll is statically loaded (as opposed to loaded
// dynamically via LoadLibrary)
EXTERN_C VOID APIENTRY
ExcHndlInit(void);


// Override the report file name.
// The file name must be encoded for the current Windows code page.
// The default code page does not support Unicode characters,
// unless you are targeting Windows Version 1903 or later.
//
// Default is prog_name.RPT, in the same directory as the main executable.
//
// You can also pass "-" for stderr.
EXTERN_C BOOL APIENTRY
ExcHndlSetLogFileNameA(const char *pszLogFileName);


// Override the report file name.
// The file name is encoded as Unicode wide characters.
//
// Default is prog_name.RPT, in the same directory as the main executable.
//
// You can also pass L"-" for stderr.
EXTERN_C BOOL APIENTRY
ExcHndlSetLogFileNameW(const WCHAR *pszLogFileName);
