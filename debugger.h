/* This is a Cfunctions (version 0.28) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from 'http://www.lemoda.net/cfunctions/'. */

/* This file was generated with:
'cfunctions -i debugger.c' */
#ifndef CFH_DEBUGGER_H
#define CFH_DEBUGGER_H

/* From 'debugger.c': */
#define DBG_EXCEPTION_HANDLED	((DWORD)0x00010001L)
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
BOOL GetPlatformId (LPDWORD lpdwPlatformId );
BOOL ObtainSeDebugPrivilege (void);
BOOL TerminateDebugee (void);
void DebugProcess (void * dummy );
BOOL DebugMainLoop (void);

#endif /* CFH_DEBUGGER_H */
