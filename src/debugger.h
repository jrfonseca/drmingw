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


#ifndef DEBUGGER_H
#define DEBUGGER_H


#include <windows.h>


typedef struct {
    DWORD dwProcessId;
    HANDLE hProcess;
}
PROCESS_LIST_INFO, * PPROCESS_LIST_INFO;

typedef struct {
    DWORD dwProcessId;
    DWORD dwThreadId;
    HANDLE hThread;
    LPVOID lpThreadLocalBase;
    LPTHREAD_START_ROUTINE lpStartAddress;
}
THREAD_LIST_INFO, * PTHREAD_LIST_INFO;

typedef struct {
    DWORD dwProcessId;
    HANDLE hFile;
    LPVOID lpBaseAddress;
    DWORD dwDebugInfoFileOffset;
    DWORD nDebugInfoSize;
    LPVOID lpImageName;
    WORD fUnicode;
}
MODULE_LIST_INFO, * PMODULE_LIST_INFO;

extern int breakpoint_flag;
extern int verbose_flag;
extern DWORD dwProcessId;
extern HANDLE hEvent;
extern unsigned nProcesses, maxProcesses;
extern PPROCESS_LIST_INFO ProcessListInfo;
extern unsigned nThreads, maxThreads;
extern PTHREAD_LIST_INFO ThreadListInfo;
extern unsigned nModules, maxModules;
extern PMODULE_LIST_INFO ModuleListInfo;

BOOL ObtainSeDebugPrivilege (void);
void DebugProcess (void * dummy );
BOOL DebugMainLoop (void);

#endif /* DEBUGGER_H */
