/* module.c
 *
 *
 * José Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>

#include "debugger.h"


// The GetModuleBase function retrieves the base address of the module that contains the specified address. 
DWORD GetModuleBase(HANDLE hProcess, DWORD dwAddress)
{
	MEMORY_BASIC_INFORMATION Buffer;
	
	return VirtualQueryEx(hProcess, (LPCVOID) dwAddress, &Buffer, sizeof(Buffer)) ? (DWORD) Buffer.AllocationBase : 0;
}


#include <psapi.h>

static HMODULE hModule_PSAPI = NULL;
typedef DWORD (WINAPI *PFNGETMODULEBASENAME)(HANDLE, HMODULE, LPTSTR, DWORD) ;
static PFNGETMODULEBASENAME pfnGetModuleBaseName = NULL;
typedef DWORD (WINAPI *PFNGETMODULEFILENAMEEX)(HANDLE, HMODULE, LPTSTR, DWORD) ;
static PFNGETMODULEFILENAMEEX pfnGetModuleFileNameEx = NULL;

static DWORD GetModuleBaseName_PSAPI(HANDLE hProcess, HMODULE hModule, LPTSTR lpBaseName, DWORD nSize)
{
	if(!pfnGetModuleBaseName)
	{
		if(!hModule_PSAPI)
			if(!(hModule_PSAPI = LoadLibrary(_T("PSAPI.DLL"))))
				return 0;
#ifdef UNICODE				
		if(!(pfnGetModuleBaseName = (PFNGETMODULEBASENAME) GetProcAddress(hModule_PSAPI, "GetModuleBaseNameExW")))
#else
		if(!(pfnGetModuleBaseName = (PFNGETMODULEBASENAME) GetProcAddress(hModule_PSAPI, "GetModuleBaseNameExA")))
#endif
				return 0;
	}
	
	return pfnGetModuleBaseName(hProcess, hModule, lpBaseName, nSize); 	
}

static DWORD GetModuleFileNameEx_PSAPI(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize)
{
	if(!pfnGetModuleFileNameEx)
	{
		if(!hModule_PSAPI)
			if(!(hModule_PSAPI = LoadLibrary(_T("PSAPI.DLL"))))
				return 0;
#ifdef UNICODE				
		if(!(pfnGetModuleFileNameEx = (PFNGETMODULEFILENAMEEX) GetProcAddress(hModule_PSAPI, "GetModuleFileNameExW")))
#else
		if(!(pfnGetModuleFileNameEx = (PFNGETMODULEFILENAMEEX) GetProcAddress(hModule_PSAPI, "GetModuleFileNameExA")))
#endif
				return 0;
	}
	
	return pfnGetModuleFileNameEx(hProcess, hModule, lpFilename, nSize); 	
}


#include <tlhelp32.h>

static HMODULE hModule_KERNEL32 = NULL;
typedef HANDLE (WINAPI *PFNCREATETOOLHELP32SNAPSHOT)(DWORD, DWORD);
static PFNCREATETOOLHELP32SNAPSHOT pfnCreateToolhelp32Snapshot = NULL;
typedef BOOL (WINAPI *PFNMODULE32FIRST)(HANDLE, LPMODULEENTRY32);
static PFNMODULE32FIRST pfnModule32First= NULL;
typedef BOOL (WINAPI *PFNMODULE32NEXT)(HANDLE, LPMODULEENTRY32);
static PFNMODULE32NEXT pfnModule32Next = NULL;
typedef BOOL (WINAPI *PFNPROCESS32FIRST)(HANDLE, LPPROCESSENTRY32);
static PFNPROCESS32FIRST pfnProcess32First= NULL;
typedef BOOL (WINAPI *PFNPROCESS32NEXT)(HANDLE, LPPROCESSENTRY32);
static PFNPROCESS32NEXT pfnProcess32Next = NULL;

static DWORD GetModuleBaseName_THELP32(HANDLE hProcess, HMODULE hModule, LPTSTR lpBaseName, DWORD nSize)
{
	DWORD dwProcessId; 
	DWORD nWritten;

	if(!pfnCreateToolhelp32Snapshot || !pfnModule32First || !pfnModule32Next)
	{
		if(!hModule_KERNEL32 && !(hModule_KERNEL32 = GetModuleHandle(_T("KERNEL32.DLL"))))
				return 0;
		if(!pfnCreateToolhelp32Snapshot && !(pfnCreateToolhelp32Snapshot = (PFNCREATETOOLHELP32SNAPSHOT) GetProcAddress(hModule_KERNEL32, "CreateToolhelp32Snapshot")))
				return 0;
		if(!pfnModule32First && !(pfnModule32First = (PFNMODULE32FIRST) GetProcAddress(hModule_KERNEL32, "Module32First")))
				return 0;
		if(!pfnModule32Next && !(pfnModule32Next = (PFNMODULE32FIRST) GetProcAddress(hModule_KERNEL32, "Module32Next")))
				return 0;
		if(!pfnProcess32First && !(pfnProcess32First = (PFNPROCESS32FIRST) GetProcAddress(hModule_KERNEL32, "Process32First")))
				return 0;
		if(!pfnProcess32Next && !(pfnProcess32Next = (PFNPROCESS32FIRST) GetProcAddress(hModule_KERNEL32, "Process32Next")))
				return 0;
	}
 
	// Find the process ID
 	{
 		unsigned i = 0;
 		
		assert(nProcesses);
		while(i < nProcesses && ProcessListInfo[i].hProcess != hProcess)
			++i;
		assert(ProcessListInfo[i].hProcess == hProcess);
		dwProcessId = ProcessListInfo[i].dwProcessId;
	}

 	nWritten = 0;

 	if(hModule == NULL)
 	{
		HANDLE hProcessSnap; 
		PROCESSENTRY32 pe32 = {0}; 
		
		//  Take a snapshot of all processes in the system. 
		if ((hProcessSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == (HANDLE) -1)
			return 0;

		//  Fill in the size of the structure before using it. 
		pe32.dwSize = sizeof(PROCESSENTRY32); 
		
		//  Walk the snapshot of the processes, and for each process, 
		//  display information. 
		if (pfnProcess32First(hProcessSnap, &pe32)) 
			do 
			{ 
				if(pe32.th32ProcessID == dwProcessId)
				{
					HANDLE hModuleSnap; 
					MODULEENTRY32 me32 = {0};
					
					// Take a snapshot of all modules in the specified process.
					if ((hModuleSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId)) == (HANDLE) -1) 
						break;

			 		// Fill the size of the structure before using it. 
					me32.dwSize = sizeof(MODULEENTRY32);

					// Walk the module list of the process, and find the module of 
					// interest. 
					if (pfnModule32First(hModuleSnap, &me32)) 
						do 
						{ 
							if (me32.th32ModuleID == pe32.th32ModuleID) 
							{
								lstrcpyn(lpBaseName, me32.szModule, nSize);
								nWritten = lstrlen(lpBaseName);
								break;
							}
						} 
						while(pfnModule32Next(hModuleSnap, &me32)); 

					// Do not forget to clean up the snapshot object. 
					CloseHandle (hModuleSnap); 
					break;
				}
			}
			while (pfnProcess32Next(hProcessSnap, &pe32)); 

		if(!nWritten)
		{
			lstrcpyn(lpBaseName, pe32.szExeFile, nSize);
			nWritten = lstrlen(lpBaseName);
		}
		
		// Do not forget to clean up the snapshot object. 
		CloseHandle (hProcessSnap); 
 	}
	else
	{
		HANDLE hModuleSnap; 
		MODULEENTRY32 me32 = {0};

		// Take a snapshot of all modules in the specified process.
		if ((hModuleSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId)) == (HANDLE) -1) 
			return 0;
 
 		// Fill the size of the structure before using it. 
		me32.dwSize = sizeof(MODULEENTRY32); 
	 
		// Walk the module list of the process, and find the module of 
		// interest. 
		if (pfnModule32First(hModuleSnap, &me32)) 
			do 
			{ 
				if (me32.hModule == hModule) 
				{
					lstrcpyn(lpBaseName, me32.szModule, nSize);
					nWritten = lstrlen(lpBaseName);
					break;
				}
			} 
			while(pfnModule32Next(hModuleSnap, &me32)); 

		// Do not forget to clean up the snapshot object. 
		CloseHandle (hModuleSnap); 
	}

	return nWritten;	
}


static DWORD GetModuleFileNameEx_THELP32(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize)
{
	DWORD dwProcessId; 
	DWORD nWritten;

	if(!pfnCreateToolhelp32Snapshot || !pfnModule32First || !pfnModule32Next)
	{
		if(!hModule_KERNEL32 && !(hModule_KERNEL32 = GetModuleHandle(_T("KERNEL32.DLL"))))
				return 0;
		if(!pfnCreateToolhelp32Snapshot && !(pfnCreateToolhelp32Snapshot = (PFNCREATETOOLHELP32SNAPSHOT) GetProcAddress(hModule_KERNEL32, "CreateToolhelp32Snapshot")))
				return 0;
		if(!pfnModule32First && !(pfnModule32First = (PFNMODULE32FIRST) GetProcAddress(hModule_KERNEL32, "Module32First")))
				return 0;
		if(!pfnModule32Next && !(pfnModule32Next = (PFNMODULE32FIRST) GetProcAddress(hModule_KERNEL32, "Module32Next")))
				return 0;
		if(!pfnProcess32First && !(pfnProcess32First = (PFNPROCESS32FIRST) GetProcAddress(hModule_KERNEL32, "Process32First")))
				return 0;
		if(!pfnProcess32Next && !(pfnProcess32Next = (PFNPROCESS32FIRST) GetProcAddress(hModule_KERNEL32, "Process32Next")))
				return 0;
	}
 
	// Find the process ID
 	{
 		unsigned i = 0;
 		
		assert(nProcesses);
		while(i < nProcesses && ProcessListInfo[i].hProcess != hProcess)
			++i;
		assert(ProcessListInfo[i].hProcess == hProcess);
		dwProcessId = ProcessListInfo[i].dwProcessId;
	}

 	nWritten = 0;

 	if(hModule == NULL)
 	{
		HANDLE hProcessSnap; 
		PROCESSENTRY32 pe32 = {0}; 
		
		//  Take a snapshot of all processes in the system. 
		if ((hProcessSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == (HANDLE) -1)
			return 0;

		//  Fill in the size of the structure before using it. 
		pe32.dwSize = sizeof(PROCESSENTRY32); 
		
		//  Walk the snapshot of the processes, and for each process, 
		//  display information. 
		if (pfnProcess32First(hProcessSnap, &pe32)) 
			do 
			{ 
				if(pe32.th32ProcessID == dwProcessId)
				{
					HANDLE hModuleSnap; 
					MODULEENTRY32 me32 = {0};
					
					// Take a snapshot of all modules in the specified process.
					if ((hModuleSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId)) == (HANDLE) -1) 
						break;

			 		// Fill the size of the structure before using it. 
					me32.dwSize = sizeof(MODULEENTRY32);

					// Walk the module list of the process, and find the module of 
					// interest. 
					if (pfnModule32First(hModuleSnap, &me32)) 
						do 
						{ 
							if (me32.th32ModuleID == pe32.th32ModuleID) 
							{
								lstrcpyn(lpFilename, me32.szExePath, nSize);
								nWritten = lstrlen(lpFilename);
								break;
							}
						} 
						while(pfnModule32Next(hModuleSnap, &me32)); 

					// Do not forget to clean up the snapshot object. 
					CloseHandle (hModuleSnap); 
					break;
				}
			}
			while (pfnProcess32Next(hProcessSnap, &pe32)); 

		if(!nWritten)
		{
			lstrcpyn(lpFilename, pe32.szExeFile, nSize);
			nWritten = lstrlen(lpFilename);
		}
		
		// Do not forget to clean up the snapshot object. 
		CloseHandle (hProcessSnap); 
 	}
	else
	{
		HANDLE hModuleSnap; 
		MODULEENTRY32 me32 = {0};

		// Take a snapshot of all modules in the specified process.
		if ((hModuleSnap = pfnCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId)) == (HANDLE) -1) 
			return 0;
 
 		// Fill the size of the structure before using it. 
		me32.dwSize = sizeof(MODULEENTRY32); 
	 
		// Walk the module list of the process, and find the module of 
		// interest. 
		if (pfnModule32First(hModuleSnap, &me32)) 
			do 
			{ 
				if (me32.hModule == hModule) 
				{
					lstrcpyn(lpFilename, me32.szExePath, nSize);
					nWritten = lstrlen(lpFilename);
					break;
				}
			} 
			while(pfnModule32Next(hModuleSnap, &me32)); 

		// Do not forget to clean up the snapshot object. 
		CloseHandle (hModuleSnap); 
	}

	return nWritten;	
}

// The GetModuleBaseName_ function retrieves the base name of the specified module.
DWORD j_GetModuleBaseName(HANDLE hProcess, HMODULE hModule, LPTSTR lpBaseName, DWORD nSize)
{
	DWORD nWritten;
	
	if((nWritten = GetModuleBaseName_THELP32(hProcess, hModule, lpBaseName, nSize)))
		return nWritten;
	
	if((nWritten = GetModuleBaseName_PSAPI(hProcess, hModule, lpBaseName, nSize)))
		return nWritten;

	return 0;
}	

// The GetModuleFileNameEx_ function retrieves the fully qualified path for the specified module. 
DWORD j_GetModuleFileNameEx(HANDLE hProcess, HMODULE hModule, LPTSTR lpFilename, DWORD nSize)
{
	DWORD nWritten;
	
	if((nWritten = GetModuleFileNameEx_THELP32(hProcess, hModule, lpFilename, nSize)))
		return nWritten;
	
	if((nWritten = GetModuleFileNameEx_PSAPI(hProcess, hModule, lpFilename, nSize)))
		return nWritten;

	return 0;
}

BOOL GetVirtualAddress(HANDLE hProcess, DWORD dwAddress, LPDWORD lpOffset)
{
	HMODULE hModule;
	PIMAGE_DOS_HEADER pDosHdr;
	PIMAGE_NT_HEADERS pNtHdr;
	LONG e_lfanew;
	DWORD ImageBase;
	
	if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)))
		return FALSE;
	
	// Point to the DOS header in memory
	pDosHdr = (PIMAGE_DOS_HEADER)hModule;
	
	// From the DOS header, find the NT (PE) header
	if(!ReadProcessMemory(hProcess, &pDosHdr->e_lfanew, &e_lfanew, sizeof(e_lfanew), NULL))
		return FALSE;
	
	pNtHdr = (PIMAGE_NT_HEADERS)((DWORD)hModule + (DWORD)e_lfanew);

	if(!ReadProcessMemory(hProcess, &pNtHdr->OptionalHeader.ImageBase, &ImageBase, sizeof(ImageBase), NULL))
		return FALSE;

	*lpOffset = dwAddress - (DWORD)hModule + ImageBase;
	
	return TRUE;
}

