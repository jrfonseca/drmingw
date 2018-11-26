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


#pragma once

#include <windows.h>


struct DebugOptions {
    bool breakpoint_flag; /* Treat breakpoints as exceptions. */
    bool verbose_flag;    /* Verbose output. */
    bool debug_flag;
    bool first_chance;
    bool minidump;
    HANDLE hEvent;       /* Signal an event after process is attached.  */
    DWORD dwThreadId;    /* Resume thread after process is attached */
};

EXTERN_C BOOL ObtainSeDebugPrivilege(void);
EXTERN_C BOOL DebugMainLoop(const DebugOptions *pOptions);
EXTERN_C BOOL TrapThread(DWORD dwProcessId, DWORD dwThreadId);
