/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i debugger.c' */
#ifndef CFH_DEBUGGER_H
#define CFH_DEBUGGER_H

/* From `debugger.c': */
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

	void *abfd;
	void *syms;	
	long symcount;	

	void *dhandle;
} 
MODULE_LIST_INFO, * PMODULE_LIST_INFO;

#if defined(i386)
#define BP_SIZE 1
#define PC(C) ((C)->Eip)

#elif defined(PPC)
#define BP_SIZE 4
#define PC(C) ((C)->Iar)

#elif defined(MIPS)
#define BP_SIZE 4
#define PC(C) ((C)->Fir)

#elif defined(ALPHA)
#define BP_SIZE 4
#define PC(C) ((C)->Fir)

#else
#error "Unknown target CPU"

#endif
extern DWORD dwProcessId;
extern HANDLE hEvent;
extern unsigned nProcesses, maxProcesses;
extern PPROCESS_LIST_INFO ProcessListInfo;
extern unsigned nThreads, maxThreads;
extern PTHREAD_LIST_INFO ThreadListInfo;
extern unsigned nModules, maxModules;
extern PMODULE_LIST_INFO ModuleListInfo;
void DebugProcess (void * dummy );
BOOL DebugMainLoop (void);

#endif /* CFH_DEBUGGER_H */
