/* debugger.c
 *
 *
 * José Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>

#include <stdlib.h>

#include <bfd.h>

#include "debugger.h"
#include "log.h"
#include "misc.h"

#ifdef HEADER

#define DBG_EXCEPTION_HANDLED	((DWORD)0x00010001L)

typedef struct
{
	DWORD dwProcessId;
	HANDLE hProcess; 
} PROCESS_LIST_INFO, *PPROCESS_LIST_INFO;

typedef struct
{
	DWORD dwProcessId;
	DWORD dwThreadId;
	HANDLE hThread; 
	LPVOID lpThreadLocalBase; 
	LPTHREAD_START_ROUTINE lpStartAddress; 
} THREAD_LIST_INFO, *PTHREAD_LIST_INFO;

typedef struct
{
	DWORD dwProcessId;
	HANDLE hFile; 
	LPVOID lpBaseAddress; 
	DWORD dwDebugInfoFileOffset; 
	DWORD nDebugInfoSize; 
	LPVOID lpImageName; 
	WORD fUnicode;

	void *abfd;
	void *syms;	// The symbol table.
	long symcount;	// Number of symbols in `syms'.

	void *dhandle;
} MODULE_LIST_INFO, *PMODULE_LIST_INFO;

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

#endif	/* HEADER */


DWORD dwProcessId;	/* Attach to the process with the given identifier.  */
HANDLE hEvent = NULL;	/* Signal an event after process is attached.  */

unsigned nProcesses = 0, maxProcesses = 0;
PPROCESS_LIST_INFO ProcessListInfo = NULL;

unsigned nThreads = 0, maxThreads = 0;
PTHREAD_LIST_INFO ThreadListInfo = NULL;

unsigned nModules = 0, maxModules = 0;
PMODULE_LIST_INFO ModuleListInfo = NULL;

void DebugProcess(void * dummy)
{
	// attach debuggee
	if(!DebugActiveProcess(dwProcessId))
		ErrorMessageBox(_T("DebugActiveProcess: %s"), LastErrorMessage());
	else
		DebugMainLoop();
}

BOOL DebugMainLoop(void)
{
	BOOL fFinished = FALSE;

	unsigned i, j;

	while(!fFinished) 
	{ 
		DEBUG_EVENT DebugEvent;			// debugging event information 
		DWORD dwContinueStatus = DBG_CONTINUE;	// exception continuation 
		
		// Wait for a debugging event to occur. The second parameter indicates 
		// that the function does not return until a debugging event occurs. 
		if(!WaitForDebugEvent(&DebugEvent, INFINITE))
		{
			ErrorMessageBox(_T("WaitForDebugEvent: %s"), LastErrorMessage());

			return FALSE;
		}

		if(verbose_flag)
			lprintf(_T("DEBUG_EVENT:\r\n"));

		// Process the debugging event code.
		switch (DebugEvent.dwDebugEventCode) 
		{ 
			case EXCEPTION_DEBUG_EVENT: 
				// Process the exception code. When handling 
				// exceptions, remember to set the continuation 
				// status parameter (dwContinueStatus). This value 
				// is used by the ContinueDebugEvent function. 
				if(verbose_flag)
				{
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("EXCEPTION_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);
					lprintf(
						_T("\tExceptionCode = %lX\r\n\tExceptionFlags = %lX\r\n\tExceptionAddress = %lX\r\n\tdwFirstChance = %lX\r\n"),
						DebugEvent.u.Exception.ExceptionRecord.ExceptionCode,
						DebugEvent.u.Exception.ExceptionRecord.ExceptionFlags,
						(DWORD) DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress, 
						DebugEvent.u.Exception.dwFirstChance
					);					
				}

				if (DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
					if (DebugEvent.u.Exception.dwFirstChance)
					{
						/*
						PPROCESS_LIST_INFO pProcessInfo;
						PTHREAD_LIST_INFO pThreadInfo;
						CONTEXT Context;
						
						// Find the process in the process list
						assert(nProcesses);
						i = 0;
						while(i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId)
							++i;
						assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);
						pProcessInfo = &ProcessListInfo[i];
						
						// Find the thread in the thread list
						assert(nThreads);
						i = 0;
						while(i < nThreads && (DebugEvent.dwProcessId > ThreadListInfo[i].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i].dwProcessId && DebugEvent.dwThreadId > ThreadListInfo[i].dwThreadId)))
							++i;
						assert(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId && ThreadListInfo[i].dwThreadId == DebugEvent.dwThreadId);
						pThreadInfo = &ThreadListInfo[i];
						
						// Get the thread context
						Context.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT | CONTEXT_SEGMENTS | CONTEXT_INTEGER | CONTEXT_CONTROL;
						if(!GetThreadContext(pThreadInfo->hThread, &Context))
							assert(0);

						// Skip over breakpoint
						PC(&Context) = (DWORD) DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress + BP_SIZE;
						SetThreadContext(pThreadInfo->hThread, &Context);
						*/
						
						// Signal the aedebug event
						if(hEvent)
						{
							SetEvent(hEvent);
							hEvent = NULL;
						}

						dwContinueStatus = DBG_CONTINUE;
						break;
					}
				}

				LogException(DebugEvent);
				
				dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
				break;
	 
			case CREATE_THREAD_DEBUG_EVENT: 
				// As needed, examine or change the thread's registers 
				// with the GetThreadContext and SetThreadContext functions; 
				// and suspend and resume thread execution with the 
				// SuspendThread and ResumeThread functions. 
				if(verbose_flag)
				{
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("CREATE_THREAD_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);
					lprintf(
						_T("\thThread = %lX\r\n\tlpThreadLocalBase = %lX\r\n\tlpStartAddress = %lX\r\n"), 
						(DWORD) DebugEvent.u.CreateThread.hThread, 
						(DWORD) DebugEvent.u.CreateThread.lpThreadLocalBase, 
						(DWORD) DebugEvent.u.CreateThread.lpStartAddress
					);
				}
				
				// Add the thread to the thread list
				if(nThreads == maxThreads)
					ThreadListInfo = (PTHREAD_LIST_INFO) (maxThreads ? realloc(ThreadListInfo, (maxThreads *= 2)*sizeof(THREAD_LIST_INFO)) : malloc((maxThreads = 4)*sizeof(THREAD_LIST_INFO)));
				i = nThreads++;
				while(i > 0 && (DebugEvent.dwProcessId < ThreadListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i - 1].dwProcessId && DebugEvent.dwThreadId < ThreadListInfo[i - 1].dwThreadId)))
				{
					ThreadListInfo[i] = ThreadListInfo[i - 1];
					--i;
				}
				ThreadListInfo[i].dwProcessId = DebugEvent.dwProcessId;
				ThreadListInfo[i].dwThreadId = DebugEvent.dwThreadId; 
				ThreadListInfo[i].hThread = DebugEvent.u.CreateThread.hThread; 
				ThreadListInfo[i].lpThreadLocalBase = DebugEvent.u.CreateThread.lpThreadLocalBase; 
				ThreadListInfo[i].lpStartAddress = DebugEvent.u.CreateThread.lpStartAddress; 
				break;
	 
			case CREATE_PROCESS_DEBUG_EVENT: 
				// As needed, examine or change the registers of the 
				// process's initial thread with the GetThreadContext and 
				// SetThreadContext functions; read from and write to the 
				// process's virtual memory with the ReadProcessMemory and 
				// WriteProcessMemory functions; and suspend and resume 
				// thread execution with the SuspendThread and ResumeThread 
				// functions. 
				if(verbose_flag)
				{
					TCHAR szBuffer[MAX_PATH];
					LPVOID lpImageName;
					
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("CREATE_PROCESS_DEBUG_EVENT"),
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);

					if(!ReadProcessMemory(DebugEvent.u.CreateProcessInfo.hProcess, DebugEvent.u.CreateProcessInfo.lpImageName, &lpImageName, sizeof(LPVOID), NULL) ||
						!ReadProcessMemory(DebugEvent.u.CreateProcessInfo.hProcess, lpImageName, szBuffer, sizeof(szBuffer), NULL))
						lstrcpyn(szBuffer, "NULL", sizeof(szBuffer));
						
					lprintf(
						_T("\thFile = %lX\r\n\thProcess = %lX\r\n\thThread = %lX\r\n\tlpBaseOfImage = %lX\r\n\tdwDebugInfoFileOffset = %lX\r\n\tnDebugInfoSize = %lX\r\n\tlpThreadLocalBase = %lX\r\n\tlpStartAddress = %lX\r\n\tlpImageName = %s\r\n\tfUnicoded = %X\r\n"),
						(DWORD) DebugEvent.u.CreateProcessInfo.hFile, 
						(DWORD) DebugEvent.u.CreateProcessInfo.hProcess, 
						(DWORD) DebugEvent.u.CreateProcessInfo.hThread, 
						(DWORD) DebugEvent.u.CreateProcessInfo.lpBaseOfImage, 
						DebugEvent.u.CreateProcessInfo.dwDebugInfoFileOffset, 
						DebugEvent.u.CreateProcessInfo.nDebugInfoSize, 
						(DWORD) DebugEvent.u.CreateProcessInfo.lpThreadLocalBase, 
						(DWORD) DebugEvent.u.CreateProcessInfo.lpStartAddress, 
						szBuffer, 
						DebugEvent.u.CreateProcessInfo.fUnicode
					);
				}

				// Add the process to the process list
				if(nProcesses == maxProcesses)
					ProcessListInfo = (PPROCESS_LIST_INFO) (maxProcesses ? realloc(ThreadListInfo, (maxProcesses *= 2)*sizeof(PROCESS_LIST_INFO)) : malloc((maxProcesses = 4)*sizeof(PROCESS_LIST_INFO)));
				i = nProcesses++;
				while(i > 0 && DebugEvent.dwProcessId < ProcessListInfo[i - 1].dwProcessId)
				{
					ProcessListInfo[i] = ProcessListInfo[i - 1];
					--i;
				}
				ProcessListInfo[i].dwProcessId = DebugEvent.dwProcessId;
				ProcessListInfo[i].hProcess = DebugEvent.u.CreateProcessInfo.hProcess; 

				// Add the initial thread of the process to the thread list
				if(nThreads == maxThreads)
					ThreadListInfo = (PTHREAD_LIST_INFO) (maxThreads ? realloc(ThreadListInfo, (maxThreads *= 2)*sizeof(THREAD_LIST_INFO)) : malloc((maxThreads = 4)*sizeof(THREAD_LIST_INFO)));
				i = nThreads++;
				while(i > 0 && (DebugEvent.dwProcessId < ThreadListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i - 1].dwProcessId && DebugEvent.dwThreadId < ThreadListInfo[i - 1].dwThreadId)))
				{
					ThreadListInfo[i] = ThreadListInfo[i - 1];
					--i;
				}
				ThreadListInfo[i].dwProcessId = DebugEvent.dwProcessId;
				ThreadListInfo[i].dwThreadId = DebugEvent.dwThreadId; 
				ThreadListInfo[i].hThread = DebugEvent.u.CreateProcessInfo.hThread; 
				ThreadListInfo[i].lpThreadLocalBase = DebugEvent.u.CreateProcessInfo.lpThreadLocalBase; 
				ThreadListInfo[i].lpStartAddress = DebugEvent.u.CreateProcessInfo.lpStartAddress; 

				// Add the initial module of the process to the module list
				if(nModules == maxModules)
					ModuleListInfo = (PMODULE_LIST_INFO) (maxModules ? realloc(ModuleListInfo, (maxModules *= 2)*sizeof(MODULE_LIST_INFO)) : malloc((maxModules = 4)*sizeof(MODULE_LIST_INFO)));
				i = nModules++;
				while(i > 0 && (DebugEvent.dwProcessId < ModuleListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ModuleListInfo[i - 1].dwProcessId && DebugEvent.u.CreateProcessInfo.lpBaseOfImage < ModuleListInfo[i - 1].lpBaseAddress)))
				{
					ModuleListInfo[i] = ModuleListInfo[i - 1];
					--i;
				}
				ModuleListInfo[i].dwProcessId = DebugEvent.dwProcessId;
				ModuleListInfo[i].hFile = DebugEvent.u.CreateProcessInfo.hFile; 
				ModuleListInfo[i].lpBaseAddress = DebugEvent.u.CreateProcessInfo.lpBaseOfImage; 
				ModuleListInfo[i].dwDebugInfoFileOffset = DebugEvent.u.CreateProcessInfo.dwDebugInfoFileOffset; 
				ModuleListInfo[i].nDebugInfoSize = DebugEvent.u.CreateProcessInfo.nDebugInfoSize; 
				ModuleListInfo[i].lpImageName = DebugEvent.u.CreateProcessInfo.lpImageName; 
				ModuleListInfo[i].fUnicode = DebugEvent.u.CreateProcessInfo.fUnicode; 
				
				ModuleListInfo[i].abfd = NULL;
				ModuleListInfo[i].syms = NULL;
				ModuleListInfo[i].symcount = 0;
			
				ModuleListInfo[i].dhandle = NULL;
				break;

			case EXIT_THREAD_DEBUG_EVENT: 
				// Display the thread's exit code. 
				if(verbose_flag)
				{
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("EXIT_THREAD_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);
					lprintf(
						_T("\tdwExitCode = %lX\r\n"),
						DebugEvent.u.ExitThread.dwExitCode 
					);
				}

				// Remove the thread from the thread list
				assert(nThreads);
				i = 0;
				while(i < nThreads && (DebugEvent.dwProcessId > ThreadListInfo[i].dwProcessId || (DebugEvent.dwProcessId == ThreadListInfo[i].dwProcessId && DebugEvent.dwThreadId > ThreadListInfo[i].dwThreadId)))
					++i;
				assert(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId && ThreadListInfo[i].dwThreadId == DebugEvent.dwThreadId);
				while(++i < nThreads)
					ThreadListInfo[i - 1] = ThreadListInfo[i];
				--nThreads;
				break;

			case EXIT_PROCESS_DEBUG_EVENT: 
				// Display the process's exit code.
				if(verbose_flag)
				{
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("EXIT_PROCESS_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);
					lprintf(
						_T("\tdwExitCode = %lX\r\n"),
						DebugEvent.u.ExitProcess.dwExitCode 
					);
				}
								
				// Remove all threads of the process from the thread list
				for(i = j = 0; i < nThreads; ++i)
				{
					if(ThreadListInfo[i].dwProcessId == DebugEvent.dwProcessId)
					{
						if(ModuleListInfo[i].syms)
							free(ModuleListInfo[i].syms);
						if(ModuleListInfo[i].abfd)
							bfd_close(ModuleListInfo[i].abfd);
						if(ModuleListInfo[i].dhandle)
							;
					}
					else
						++j;

					if(j != i)
						ThreadListInfo[j] = ThreadListInfo[i];
				}
				nThreads = j;
				
				// Remove all modules of the process from the module list
				for(i = j = 0; i < nModules; ++i)
				{
					if(ModuleListInfo[i].dwProcessId != DebugEvent.dwProcessId)
						++j;
					if(j != i)
						ModuleListInfo[j] = ModuleListInfo[i];
				}
				nModules = j;

				// Remove the process from the process list
				assert(nProcesses);
				i = 0;
				while(i < nProcesses && DebugEvent.dwProcessId > ProcessListInfo[i].dwProcessId)
					++i;
				assert(ProcessListInfo[i].dwProcessId == DebugEvent.dwProcessId);
				while(++i < nProcesses)
					ProcessListInfo[i - 1] = ProcessListInfo[i];
				
				if(!--nProcesses)
				{
					assert(!nThreads && !nModules);
					fFinished = TRUE;
				}
				break;

			case LOAD_DLL_DEBUG_EVENT: 
				// Read the debugging information included in the newly 
				// loaded DLL. 
				if(verbose_flag)
				{
					TCHAR szBuffer[MAX_PATH];
					LPVOID lpImageName;
					
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("LOAD_DLL_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);

					if(!ReadProcessMemory(DebugEvent.u.CreateProcessInfo.hProcess, DebugEvent.u.CreateProcessInfo.lpImageName, &lpImageName, sizeof(LPVOID), NULL) ||
						!ReadProcessMemory(DebugEvent.u.CreateProcessInfo.hProcess, lpImageName, szBuffer, sizeof(szBuffer), NULL))
						lstrcpyn(szBuffer, "NULL", sizeof(szBuffer));
						
					lprintf(
						_T("\thFile = %lX\r\n\tlpBaseOfDll = %lX\r\n\tdwDebugInfoFileOffset = %lX\r\n\tnDebugInfoSize = %lX\r\n\tlpImageName = %s\r\n\tfUnicoded = %X\r\n"),
						(DWORD) DebugEvent.u.LoadDll.hFile, 
						(DWORD) DebugEvent.u.LoadDll.lpBaseOfDll, 
						DebugEvent.u.LoadDll.dwDebugInfoFileOffset, 
						DebugEvent.u.LoadDll.nDebugInfoSize, 
						szBuffer, 
						DebugEvent.u.LoadDll.fUnicode
					);
				}

				// Add the module to the module list
				if(nModules == maxModules)
					ModuleListInfo = (PMODULE_LIST_INFO) (maxModules ? realloc(ModuleListInfo, (maxModules *= 2)*sizeof(MODULE_LIST_INFO)) : malloc((maxModules = 4)*sizeof(MODULE_LIST_INFO)));
				i = nModules++;
				while(i > 0 && (DebugEvent.dwProcessId < ModuleListInfo[i - 1].dwProcessId || (DebugEvent.dwProcessId == ModuleListInfo[i - 1].dwProcessId && DebugEvent.u.LoadDll.lpBaseOfDll < ModuleListInfo[i - 1].lpBaseAddress)))
				{
					ModuleListInfo[i] = ModuleListInfo[i - 1];
					--i;
				}
				ModuleListInfo[i].dwProcessId = DebugEvent.dwProcessId;
				ModuleListInfo[i].hFile = DebugEvent.u.LoadDll.hFile; 
				ModuleListInfo[i].lpBaseAddress = DebugEvent.u.LoadDll.lpBaseOfDll; 
				ModuleListInfo[i].dwDebugInfoFileOffset = DebugEvent.u.LoadDll.dwDebugInfoFileOffset; 
				ModuleListInfo[i].nDebugInfoSize = DebugEvent.u.LoadDll.nDebugInfoSize; 
				ModuleListInfo[i].lpImageName = DebugEvent.u.LoadDll.lpImageName; 
				ModuleListInfo[i].fUnicode = DebugEvent.u.LoadDll.fUnicode; 

				ModuleListInfo[i].abfd = NULL;
				ModuleListInfo[i].syms = NULL;
				ModuleListInfo[i].symcount = 0;
			
				ModuleListInfo[i].dhandle = NULL;
				break;
	 
			case UNLOAD_DLL_DEBUG_EVENT: 
				// Display a message that the DLL has been unloaded. 
				if(verbose_flag)
				{
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("UNLOAD_DLL_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);
					lprintf(
						_T("\tlpBaseOfDll = %lX\r\n"),
						(DWORD) DebugEvent.u.UnloadDll.lpBaseOfDll 
					);
				}
				
				// Remove the module from the module list
				assert(nModules);
				i = 0;
				while(i < nModules && (DebugEvent.dwProcessId > ModuleListInfo[i].dwProcessId || (DebugEvent.dwProcessId == ModuleListInfo[i].dwProcessId && DebugEvent.u.UnloadDll.lpBaseOfDll > ModuleListInfo[i].lpBaseAddress)))
					++i;
				assert(ModuleListInfo[i].dwProcessId == DebugEvent.dwProcessId && ModuleListInfo[i].lpBaseAddress == DebugEvent.u.UnloadDll.lpBaseOfDll);

				if(ModuleListInfo[i].syms)
					free(ModuleListInfo[i].syms);
				if(ModuleListInfo[i].abfd)
					bfd_close(ModuleListInfo[i].abfd);
				if(ModuleListInfo[i].dhandle)
					;

				while(++i < nModules)
					ModuleListInfo[i - 1] = ModuleListInfo[i];
				--nModules;
				assert(nModules);
				break;
	 
			case OUTPUT_DEBUG_STRING_EVENT: 
				// Display the output debugging string. 
				if(verbose_flag)
				{
					lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("OUTPUT_DEBUG_STRING_EVENT"),
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);
					lprintf(
						_T("\tlpDebugStringData = %s\r\n"),
						DebugEvent.u.DebugString.lpDebugStringData 
					);
				}
				break;
	 		
	 		default:
	 			if(verbose_flag)
	 				lprintf(
						_T("\tdwDebugEventCode = %s\r\n\tdwProcessId = %lX\r\n\tdwThreadId = %lX\r\n"),
						_T("UNLOAD_DLL_DEBUG_EVENT"), 
						DebugEvent.dwProcessId, 
						DebugEvent.dwThreadId
					);

		} 
	 
		// Resume executing the thread that reported the debugging event. 
		ContinueDebugEvent(
			DebugEvent.dwProcessId, 
			DebugEvent.dwThreadId, 
			dwContinueStatus
		);
	}
	
	
	return TRUE;
}
