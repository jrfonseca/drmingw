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


typedef struct {
    int breakpoint_flag; /* Treat breakpoints as exceptions. */
    int verbose_flag;    /* Verbose output. */
    int debug_flag;
    int first_chance;
    DWORD dwProcessId;   /* Attach to the process with the given identifier.  */
    HANDLE hEvent;       /* Signal an event after process is attached.  */
} DebugOptions;

EXTERN_C BOOL ObtainSeDebugPrivilege(void);
EXTERN_C BOOL DebugMainLoop(const DebugOptions *pOptions);
