/* symbols.c
 *
 *
 * José Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>
#include <stdlib.h>

#include "debugger.h"
#include "log.h"
#include "misc.h"
#include "module.h"
#include "symbols.h"

#ifdef HEADER
#define MAX_SYM_NAME_SIZE	4096

#include <bfd.h>
#endif /* HEADER */

#include <libiberty.h>
#include <demangle.h>
#include "coff/internal.h"
#include "budbg.h"
#include "debug.h"
#include "libcoff.h"

// Read in the symbol table.
static boolean
slurp_symtab (bfd *abfd, asymbol ***syms, long *symcount)
{
	long storage;

	if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
		return false;

	storage = bfd_get_symtab_upper_bound (abfd);
	if (storage < 0)
		return false;

	*syms = (asymbol **) xmalloc (storage);

	if((*symcount = bfd_canonicalize_symtab (abfd, *syms)) < 0)
		return false;
	
	return true;
}

// This stucture is used to pass information between translate_addresses and find_address_in_section.
struct find_handle
{
	asymbol **syms;
	bfd_vma pc;
	const char *filename;
	const char *functionname;
	unsigned int line;
	boolean found;
};

// Look for an address in a section.  This is called via  bfd_map_over_sections. 
static void find_address_in_section (bfd *abfd, asection *section, PTR data)
{
	struct find_handle *info = (struct find_handle *) data;
	bfd_vma vma;
	bfd_size_type size;

	if (info->found)
		return;

	if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
		return;

	if (info->pc < (vma = bfd_get_section_vma (abfd, section)))
		return;

	if (info->pc >= vma + (size = bfd_get_section_size_before_reloc (section)))
		return;

	info->found = bfd_find_nearest_line (abfd, section, info->syms, info->pc - vma, &info->filename, &info->functionname, &info->line);
}

BOOL BfdDemangleSymName(LPCTSTR lpName, LPTSTR lpDemangledName, DWORD nSize)
{
	char *res;
	
	assert(lpName != NULL);
	
	if((res = cplus_demangle(lpName, DMGL_ANSI /*| DMGL_PARAMS*/)) == NULL)
	{
		lstrcpyn(lpDemangledName, lpName, nSize);
		return FALSE;
	}
	else
	{
		lstrcpyn(lpDemangledName, res, nSize);
		free (res);
		return TRUE;
	}
}

BOOL BfdGetSymFromAddr(bfd *abfd, asymbol **syms, long symcount, HANDLE hProcess, DWORD dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	HMODULE hModule;
	struct find_handle info;
	
	if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)))
		return FALSE;
	
	info.pc = dwAddress;

	if(!(bfd_get_file_flags (abfd) & HAS_SYMS) || !symcount)
	{
		if(verbose_flag)
			lprintf(_T("%s: %s\r\n"), bfd_get_filename (abfd), _T("No symbols"));
		return FALSE;
	}
	info.syms = syms;

	info.found = false;
	bfd_map_over_sections (abfd, find_address_in_section, (PTR) &info);
	if (info.found == false || info.line == 0)
	{
		if(verbose_flag)
			lprintf(_T("%s: %s\r\n"), bfd_get_filename (abfd), _T("No symbol found"));
		return FALSE;
	}

	assert(lpSymName);
	
	if(info.functionname == NULL && *info.functionname == '\0')
		return FALSE;		
	
	lstrcpyn(lpSymName, info.functionname, nSize);

	return TRUE;
}

BOOL BfdGetLineFromAddr(bfd *abfd, asymbol **syms, long symcount, HANDLE hProcess, DWORD dwAddress,  LPTSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
	HMODULE hModule;
	struct find_handle info;
	
	if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)))
		return FALSE;
	
	info.pc = dwAddress;

	if(!(bfd_get_file_flags (abfd) & HAS_SYMS) || !symcount)
	{
		if(verbose_flag)
			lprintf(_T("%s: %s\r\n"), bfd_get_filename (abfd), _T("No symbols"));
		return FALSE;
	}
	info.syms = syms;

	info.found = false;
	bfd_map_over_sections (abfd, find_address_in_section, (PTR) &info);
	if (info.found == false || info.line == 0)
	{
		if(verbose_flag)
			lprintf(_T("%s: %s\r\n"), bfd_get_filename (abfd), _T("No symbol found"));
		return FALSE;
	}

	assert(lpFileName && lpLineNumber);

	lstrcpyn(lpFileName, info.filename, nSize);
	*lpLineNumber = info.line;

	return TRUE;
}


#ifdef HEADER
#include <imagehlp.h>
#endif /* HEADER */

BOOL bSymInitialized = FALSE;

static HMODULE hModule_Imagehlp = NULL;

typedef BOOL (WINAPI *PFNSYMINITIALIZE)(HANDLE, LPSTR, BOOL);
static PFNSYMINITIALIZE pfnSymInitialize = NULL;

BOOL WINAPI j_SymInitialize(HANDLE hProcess, PSTR UserSearchPath, BOOL fInvadeProcess)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymInitialize || (pfnSymInitialize = (PFNSYMINITIALIZE) GetProcAddress(hModule_Imagehlp, "SymInitialize")))
	)
		return pfnSymInitialize(hProcess, UserSearchPath, fInvadeProcess);
	else
		return FALSE;
}

typedef BOOL (WINAPI *PFNSYMCLEANUP)(HANDLE);
static PFNSYMCLEANUP pfnSymCleanup = NULL;

BOOL WINAPI j_SymCleanup(HANDLE hProcess)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymCleanup || (pfnSymCleanup = (PFNSYMCLEANUP) GetProcAddress(hModule_Imagehlp, "SymCleanup")))
	)
		return pfnSymCleanup(hProcess);
	else
		return FALSE;
}

typedef DWORD (WINAPI *PFNSYMSETOPTIONS)(DWORD);
static PFNSYMSETOPTIONS pfnSymSetOptions = NULL;

static
DWORD WINAPI j_SymSetOptions(DWORD SymOptions)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymSetOptions || (pfnSymSetOptions = (PFNSYMSETOPTIONS) GetProcAddress(hModule_Imagehlp, "SymSetOptions")))
	)
		return pfnSymSetOptions(SymOptions);
	else
		return FALSE;
}

typedef BOOL (WINAPI *PFNSYMUNDNAME)(PIMAGEHLP_SYMBOL, PSTR, DWORD);
static PFNSYMUNDNAME pfnSymUnDName = NULL;

static
BOOL WINAPI j_SymUnDName(PIMAGEHLP_SYMBOL Symbol, PSTR UnDecName, DWORD UnDecNameLength)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymUnDName || (pfnSymUnDName = (PFNSYMUNDNAME) GetProcAddress(hModule_Imagehlp, "SymUnDName")))
	)
		return pfnSymUnDName(Symbol, UnDecName, UnDecNameLength);
	else
		return FALSE;
}

typedef DWORD (WINAPI *PFNUNDECORATESYMBOLNAME)(PCSTR, PSTR, DWORD, DWORD);
static PFNUNDECORATESYMBOLNAME pfnUnDecorateSymbolName = NULL;

static
DWORD j_UnDecorateSymbolName(PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnUnDecorateSymbolName || (pfnUnDecorateSymbolName = (PFNUNDECORATESYMBOLNAME) GetProcAddress(hModule_Imagehlp, "UnDecorateSymbolName")))
	)
		return pfnUnDecorateSymbolName(DecoratedName, UnDecoratedName, UndecoratedLength, Flags);
	else
		return FALSE;
}

typedef PFUNCTION_TABLE_ACCESS_ROUTINE PFNSYMFUNCTIONTABLEACCESS;
static PFNSYMFUNCTIONTABLEACCESS pfnSymFunctionTableAccess = NULL;

PVOID WINAPI j_SymFunctionTableAccess(HANDLE hProcess, DWORD AddrBase)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymFunctionTableAccess || (pfnSymFunctionTableAccess = (PFNSYMFUNCTIONTABLEACCESS) GetProcAddress(hModule_Imagehlp, "SymFunctionTableAccess")))
	)
		return pfnSymFunctionTableAccess(hProcess, AddrBase);
	else
		return NULL;
}

typedef PGET_MODULE_BASE_ROUTINE PFNSYMGETMODULEBASE;
static PFNSYMGETMODULEBASE pfnSymGetModuleBase = NULL;

DWORD WINAPI j_SymGetModuleBase(HANDLE hProcess, DWORD dwAddr)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymGetModuleBase || (pfnSymGetModuleBase = (PFNSYMGETMODULEBASE) GetProcAddress(hModule_Imagehlp, "SymGetModuleBase")))
	)
		return pfnSymGetModuleBase(hProcess, dwAddr);
	else
		return 0;
}

typedef BOOL (WINAPI *PFNSTACKWALK)(DWORD, HANDLE, HANDLE, LPSTACKFRAME, LPVOID, PREAD_PROCESS_MEMORY_ROUTINE, PFUNCTION_TABLE_ACCESS_ROUTINE, PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE);
static PFNSTACKWALK pfnStackWalk = NULL;

BOOL WINAPI j_StackWalk(
	DWORD MachineType, 
	HANDLE hProcess, 
	HANDLE hThread, 
	LPSTACKFRAME StackFrame, 
	PVOID ContextRecord, 
	PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,  
	PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine, 
	PTRANSLATE_ADDRESS_ROUTINE TranslateAddress 
)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnStackWalk || (pfnStackWalk = (PFNSTACKWALK) GetProcAddress(hModule_Imagehlp, "StackWalk")))
	)
		return pfnStackWalk(
			MachineType, 
			hProcess, 
			hThread, 
			StackFrame, 
			ContextRecord, 
			ReadMemoryRoutine,  
			FunctionTableAccessRoutine,
			GetModuleBaseRoutine, 
			TranslateAddress 
		);
	else
		return FALSE;
}

typedef BOOL (WINAPI *PFNSYMGETSYMFROMADDR)(HANDLE, DWORD, LPDWORD, PIMAGEHLP_SYMBOL);
static PFNSYMGETSYMFROMADDR pfnSymGetSymFromAddr = NULL;

BOOL WINAPI j_SymGetSymFromAddr(HANDLE hProcess, DWORD Address, PDWORD Displacement, PIMAGEHLP_SYMBOL Symbol)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymGetSymFromAddr || (pfnSymGetSymFromAddr = (PFNSYMGETSYMFROMADDR) GetProcAddress(hModule_Imagehlp, "SymGetSymFromAddr")))
	)
		return pfnSymGetSymFromAddr(hProcess, Address, Displacement, Symbol);
	else
		return FALSE;
}

typedef BOOL (WINAPI *PFNSYMGETLINEFROMADDR)(HANDLE, DWORD, LPDWORD, PIMAGEHLP_LINE);
static PFNSYMGETLINEFROMADDR pfnSymGetLineFromAddr = NULL;

BOOL WINAPI j_SymGetLineFromAddr(HANDLE hProcess, DWORD dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE Line)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("IMAGEHLP.DLL")))) &&
		(pfnSymGetLineFromAddr || (pfnSymGetLineFromAddr = (PFNSYMGETLINEFROMADDR) GetProcAddress(hModule_Imagehlp, "SymGetLineFromAddr")))
	)
		return pfnSymGetLineFromAddr(hProcess, dwAddr, pdwDisplacement, Line);
	else
		return FALSE;
}

BOOL ImagehlpDemangleSymName(LPCTSTR lpName, LPTSTR lpDemangledName, DWORD nSize)
{
	TCHAR *szDemangledName = alloca(nSize*sizeof(TCHAR));
	
	if(!j_UnDecorateSymbolName(lpName, szDemangledName, nSize, UNDNAME_COMPLETE | UNDNAME_32_BIT_DECODE))
	{
		if(verbose_flag)
			lprintf(_T("UnDecorateSymbolName: %s\r\n"), LastErrorMessage());
		*lpDemangledName = _T('\0');
		return FALSE;
	}
	
	lstrcpyn(lpDemangledName, szDemangledName, nSize);
	
	return TRUE;
}

BOOL ImagehlpGetSymFromAddr(HANDLE hProcess, DWORD dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	// IMAGEHLP is wacky, and requires you to pass in a pointer to a
	// IMAGEHLP_SYMBOL structure.  The problem is that this structure is
	// variable length.  That is, you determine how big the structure is
	// at runtime.  This means that you can't use sizeof(struct).
	// So...make a buffer that's big enough, and make a pointer
	// to the buffer.  We also need to initialize not one, but TWO
	// members of the structure before it can be used.
	
	BYTE symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + MAX_SYM_NAME_SIZE];
	PIMAGEHLP_SYMBOL pSymbol = (PIMAGEHLP_SYMBOL) symbolBuffer;
	DWORD dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

	pSymbol->SizeOfStruct = sizeof(symbolBuffer);
	pSymbol->MaxNameLength = MAX_SYM_NAME_SIZE;

	assert(bSymInitialized);
	
	if(!j_SymGetSymFromAddr(hProcess, dwAddress, &dwDisplacement, pSymbol))
		return FALSE;

	lstrcpyn(lpSymName, pSymbol->Name, nSize);

	return TRUE;
}

BOOL ImagehlpGetLineFromAddr(HANDLE hProcess, DWORD dwAddress,  LPTSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
	IMAGEHLP_LINE Line;
	DWORD dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

	// Do the source and line lookup.
	memset(&Line, 0, sizeof(IMAGEHLP_LINE));
	Line.SizeOfStruct = sizeof(IMAGEHLP_LINE);
	
	assert(bSymInitialized);

#if 1
	{
		// The problem is that the symbol engine only finds those source
		//  line addresses (after the first lookup) that fall exactly on
		//  a zero displacement.  I will walk backwards 100 bytes to
		//  find the line and return the proper displacement.
		DWORD dwTempDisp = 0 ;
		while (dwTempDisp < 100 && !j_SymGetLineFromAddr(hProcess, dwAddress - dwTempDisp, &dwDisplacement, &Line))
			++dwTempDisp;
		
		if(dwTempDisp >= 100)
			return FALSE;
			
		// It was found and the source line information is correct so
		//  change the displacement if it was looked up multiple times.
		if (dwTempDisp < 100 && dwTempDisp != 0 )
			dwDisplacement = dwTempDisp;
	}
#else
	if(!j_SymGetLineFromAddr(hProcess, dwAddress, &dwDisplacement, &Line))
		return FALSE;
#endif

	assert(lpFileName && lpLineNumber);
	
	lstrcpyn(lpFileName, Line.FileName, nSize);
	*lpLineNumber = Line.LineNumber;
	
	return TRUE;
}

BOOL PEGetSymFromAddr(HANDLE hProcess, DWORD dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	HMODULE hModule;
	PIMAGE_NT_HEADERS pNtHdr;
	IMAGE_NT_HEADERS NtHdr;
	PIMAGE_SECTION_HEADER pSection;
	DWORD dwNearestAddress = 0, dwNearestName;
	int i;

	if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)))
		return FALSE;
	
	{
		PIMAGE_DOS_HEADER pDosHdr;
		LONG e_lfanew;
		
		// Point to the DOS header in memory
		pDosHdr = (PIMAGE_DOS_HEADER)hModule;
		
		// From the DOS header, find the NT (PE) header
		if(!ReadProcessMemory(hProcess, &pDosHdr->e_lfanew, &e_lfanew, sizeof(e_lfanew), NULL))
			return FALSE;
		
		pNtHdr = (PIMAGE_NT_HEADERS)((DWORD)hModule + (DWORD)e_lfanew);
	
		if(!ReadProcessMemory(hProcess, pNtHdr, &NtHdr, sizeof(IMAGE_NT_HEADERS), NULL))
			return FALSE;
	}
	
	pSection = (PIMAGE_SECTION_HEADER) ((DWORD)pNtHdr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + NtHdr.FileHeader.SizeOfOptionalHeader);

	/*if(verbose_flag)
		lprintf(_T("Exported symbols:\r\n"));*/

	// Look for export section
	for (i = 0; i < NtHdr.FileHeader.NumberOfSections; i++, pSection++)
	{
		IMAGE_SECTION_HEADER Section;
		PIMAGE_EXPORT_DIRECTORY pExportDir = NULL;
		BYTE ExportSectionName[IMAGE_SIZEOF_SHORT_NAME] = {'.', 'e', 'd', 'a', 't', 'a', '\0', '\0'};
		
		if(!ReadProcessMemory(hProcess, pSection, &Section, sizeof(IMAGE_SECTION_HEADER), NULL))
			return FALSE;
    	
		if(memcmp(Section.Name, ExportSectionName, IMAGE_SIZEOF_SHORT_NAME) == 0)
			pExportDir = (PIMAGE_EXPORT_DIRECTORY) Section.VirtualAddress;
		else if ((NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress >= Section.VirtualAddress) && (NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress < (Section.VirtualAddress + Section.SizeOfRawData)))
			pExportDir = (PIMAGE_EXPORT_DIRECTORY) NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

		if(pExportDir)
		{
			IMAGE_EXPORT_DIRECTORY ExportDir;
			
			if(!ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + (DWORD)pExportDir), &ExportDir, sizeof(IMAGE_EXPORT_DIRECTORY), NULL))
				return FALSE;
			
			{
				PDWORD *AddressOfFunctions = alloca(ExportDir.NumberOfFunctions*sizeof(PDWORD));
				int j;
	
				if(!ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + (DWORD)ExportDir.AddressOfFunctions), AddressOfFunctions, ExportDir.NumberOfFunctions*sizeof(PDWORD), NULL))
						return FALSE;
				
				for(j = 0; j < ExportDir.NumberOfNames; ++j)
				{
					DWORD pFunction = (DWORD)hModule + (DWORD)AddressOfFunctions[j];
					//ReadProcessMemory(hProcess, (DWORD) hModule + (DWORD) (&ExportDir.AddressOfFunctions[j]), &pFunction, sizeof(pFunction), NULL);
					
					if(pFunction <= dwAddress && pFunction > dwNearestAddress)
					{
						dwNearestAddress = pFunction;
						
						if(!ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + (DWORD)(&ExportDir.AddressOfNames[j])), &dwNearestName, sizeof(dwNearestName), NULL))
							return FALSE;
							
						dwNearestName = (DWORD) hModule + dwNearestName;
					}
				
					/*if(verbose_flag)
					{
						DWORD dwName;
						char szName[256];
						
						if(ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + (DWORD)(&ExportDir.AddressOfNames[j])), &dwName, sizeof(dwName), NULL))
							if(ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + dwName), szName, sizeof(szName), NULL))
								lprintf("\t%08x\t%s\r\n", pFunction, szName);
					}*/
				}
			}
		}		
    }

	if(!dwNearestAddress)
		return FALSE;
		
	if(!ReadProcessMemory(hProcess, (PVOID)dwNearestName, lpSymName, nSize, NULL))
		return FALSE;
	lpSymName[nSize - 1] = 0;

	return TRUE;
}

BOOL GetSymFromAddr(HANDLE hProcess, DWORD dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	BOOL bReturn = FALSE;
	
	{
		HMODULE hModule;
		
		TCHAR szModule[MAX_PATH]; 
		bfd_error_handler_type old_bfd_error_handler;
		bfd *abfd;
	
		if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)) || !j_GetModuleFileNameEx(hProcess, hModule, szModule, sizeof(szModule)))
			return FALSE;

		old_bfd_error_handler = bfd_set_error_handler((bfd_error_handler_type) lprintf);
			
		if((abfd = bfd_openr (szModule, NULL)))
		{
			if(bfd_check_format(abfd, bfd_object))
			{
				bfd_vma adjust_section_vma = 0;
	
				/* If we are adjusting section VMA's, change them all now.  Changing
				the BFD information is a hack.  However, we must do it, or
				bfd_find_nearest_line will not do the right thing.  */
				if ((adjust_section_vma = (bfd_vma) hModule - pe_data(abfd)->pe_opthdr.ImageBase))
				{
					asection *s;
				
					for (s = abfd->sections; s != NULL; s = s->next)
					{
						s->vma += adjust_section_vma;
						s->lma += adjust_section_vma;
					}
				}
				
				if(bfd_get_file_flags(abfd) & HAS_SYMS)
				{
					asymbol **syms = NULL;	// The symbol table.
					long symcount = 0;	// Number of symbols in `syms'.
	
					/* Read in the symbol table.  */
					if(slurp_symtab(abfd, &syms, &symcount))
					{
						if((bReturn = BfdGetSymFromAddr(abfd, syms, symcount, hProcess, dwAddress, lpSymName, nSize)))
							BfdDemangleSymName(lpSymName, lpSymName, nSize);
	
						free(syms);
					}
					else
					{
						if(verbose_flag)
							lprintf(_T("%s: %s\r\n"), szModule, bfd_errmsg(bfd_get_error()));
					}
				}
				else
				{
					if(verbose_flag)
						lprintf(_T("%s: %s\r\n"), szModule, _T("No symbols"));
				}
				
				bfd_close(abfd);
			}
			else
			{
				if(verbose_flag)
					lprintf(_T("%s: %s\r\n"), szModule, _T("No symbols"));
			}
		}
		
		bfd_set_error_handler(old_bfd_error_handler);
	}

	if(!bReturn)
	{
		if(bSymInitialized)		
			bReturn = ImagehlpGetSymFromAddr(hProcess, dwAddress, lpSymName, nSize);
		else
		{
			j_SymSetOptions(/* SYMOPT_UNDNAME | */ SYMOPT_LOAD_LINES);
			if(j_SymInitialize(hProcess, NULL, TRUE))
			{
				bSymInitialized = TRUE;
				
				bReturn = ImagehlpGetSymFromAddr(hProcess, dwAddress, lpSymName, nSize);
				
				if(!j_SymCleanup(hProcess))
					assert(0);
					
				bSymInitialized = FALSE;
			}
			else
				if(verbose_flag)					
					lprintf(_T("SymInitialize: %s\r\n"), LastErrorMessage());
		}
	}
	
	return bReturn;
}	

BOOL GetLineFromAddr(HANDLE hProcess, DWORD dwAddress,  LPTSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
	BOOL bReturn = FALSE;
	
	{
		HMODULE hModule;
		bfd_error_handler_type old_bfd_error_handler;
		TCHAR szModule[MAX_PATH]; 
		bfd *abfd;
	
		if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)) || !j_GetModuleFileNameEx(hProcess, hModule, szModule, sizeof(szModule)))
			return FALSE;

		old_bfd_error_handler = bfd_set_error_handler((bfd_error_handler_type) lprintf);
	
		if((abfd = bfd_openr (szModule, NULL)))
		{
			if(bfd_check_format(abfd, bfd_object))
			{
				bfd_vma adjust_section_vma = 0;
	
				/* If we are adjusting section VMA's, change them all now.  Changing
				the BFD information is a hack.  However, we must do it, or
				bfd_find_nearest_line will not do the right thing.  */
				if ((adjust_section_vma = (bfd_vma) hModule - pe_data(abfd)->pe_opthdr.ImageBase))
				{
					asection *s;
				
					for (s = abfd->sections; s != NULL; s = s->next)
					{
						s->vma += adjust_section_vma;
						s->lma += adjust_section_vma;
					}
				}
				
				if(bfd_get_file_flags(abfd) & HAS_SYMS)
				{
					asymbol **syms;	// The symbol table.
					long symcount = 0;	// Number of symbols in `syms'.
	
					/* Read in the symbol table.  */
					if(slurp_symtab(abfd, &syms, &symcount))
					{
						bReturn = BfdGetLineFromAddr(abfd, syms, symcount, hProcess, dwAddress, lpFileName, nSize, lpLineNumber);
	
						free(syms);
					}
					else
					{
						if(verbose_flag)
							lprintf(_T("%s: %s\r\n"), szModule, bfd_errmsg(bfd_get_error()));
					}
				}
				else
				{
					if(verbose_flag)
						lprintf(_T("%s: %s\r\n"), szModule, _T("No symbols"));
				}
				
				bfd_close(abfd);
			}
			else
			{
				if(verbose_flag)
					lprintf(_T("%s: %s\r\n"), szModule, _T("No symbols"));
			}

		}

		bfd_set_error_handler(old_bfd_error_handler);
	}
	
	if(!bReturn)
	{
		if(bSymInitialized)		
			bReturn = ImagehlpGetLineFromAddr(hProcess, dwAddress, lpFileName, nSize, lpLineNumber);
		else
		{
			j_SymSetOptions(/* SYMOPT_UNDNAME | */ SYMOPT_LOAD_LINES);
			if(j_SymInitialize(hProcess, NULL, TRUE))
			{
				bSymInitialized = TRUE;
				
				bReturn = ImagehlpGetLineFromAddr(hProcess, dwAddress, lpFileName, nSize, lpLineNumber);

				
				if(!j_SymCleanup(hProcess))
					assert(0);
					
				bSymInitialized = FALSE;
			}
			else
				if(verbose_flag)
					lprintf(_T("SymInitialize: %s\r\n"), LastErrorMessage());
		}
	}
	
	return bReturn;
}	

BOOL WINAPI IntelStackWalk(
	DWORD MachineType, 
	HANDLE hProcess, 
	HANDLE hThread, 
	LPSTACKFRAME StackFrame, 
	PCONTEXT ContextRecord, 
	PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,  
	PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine, 
	PTRANSLATE_ADDRESS_ROUTINE TranslateAddress 
)
{
	assert(MachineType == IMAGE_FILE_MACHINE_I386);
	
	if(ReadMemoryRoutine == NULL)
		ReadMemoryRoutine = ReadProcessMemory;
	
	if(!StackFrame->Reserved[0])
	{
		StackFrame->Reserved[0] = 1;
		
		StackFrame->AddrPC.Mode = AddrModeFlat;
		StackFrame->AddrPC.Offset = ContextRecord->Eip;
		StackFrame->AddrStack.Mode = AddrModeFlat;
		StackFrame->AddrStack.Offset = ContextRecord->Esp;
		StackFrame->AddrFrame.Mode = AddrModeFlat;
		StackFrame->AddrFrame.Offset = ContextRecord->Ebp;

		StackFrame->AddrReturn.Mode = AddrModeFlat;
		if(!ReadMemoryRoutine(hProcess, (LPCVOID) (StackFrame->AddrFrame.Offset + sizeof(DWORD)), &StackFrame->AddrReturn.Offset, sizeof(DWORD), NULL))
			return FALSE;
	}
	else
	{
		StackFrame->AddrPC.Offset = StackFrame->AddrReturn.Offset;
		//AddrStack = AddrFrame + 2*sizeof(DWORD);
		if(!ReadMemoryRoutine(hProcess, (LPCVOID) StackFrame->AddrFrame.Offset, &StackFrame->AddrFrame.Offset, sizeof(DWORD), NULL))
			return FALSE;
		if(!ReadMemoryRoutine(hProcess, (LPCVOID) (StackFrame->AddrFrame.Offset + sizeof(DWORD)), &StackFrame->AddrReturn.Offset, sizeof(DWORD), NULL))
			return FALSE;
	}

	ReadMemoryRoutine(hProcess, (LPCVOID) (StackFrame->AddrFrame.Offset + 2*sizeof(DWORD)), StackFrame->Params, sizeof(StackFrame->Params), NULL);
	
	return TRUE;	
}

#include "debug.h"
extern int get_line_from_addr(bfd *abfd, asymbol **syms, long symcount, PTR dhandle, bfd_vma address, char *filename, unsigned int nsize, unsigned int *lineno);
extern int print_function_info(bfd *abfd, asymbol **syms, long symcount, PTR dhandle, HANDLE hprocess, const char *function_name, DWORD framepointer);

BOOL StackBackTrace(HANDLE hProcess, HANDLE hThread, PCONTEXT pContext)
{
	int i;
	PPROCESS_LIST_INFO pProcessListInfo;

	STACKFRAME StackFrame;

	HMODULE hModule = NULL;
	TCHAR szModule[MAX_PATH]; 

	// Remove the process from the process list
	assert(nProcesses);
	i = 0;
	while(hProcess != ProcessListInfo[i].hProcess)
	{
		++i;
		assert(i < nProcesses);
	}
	pProcessListInfo = &ProcessListInfo[i];
	
	assert(!bSymInitialized);

	j_SymSetOptions(/* SYMOPT_UNDNAME | */ SYMOPT_LOAD_LINES);
	if(j_SymInitialize(hProcess, NULL, TRUE))
		bSymInitialized = TRUE;
	else
		if(verbose_flag)
			lprintf(_T("SymInitialize: %s\r\n"), LastErrorMessage());
	
	memset( &StackFrame, 0, sizeof(StackFrame) );

	// Initialize the STACKFRAME structure for the first call.  This is only
	// necessary for Intel CPUs, and isn't mentioned in the documentation.
	StackFrame.AddrPC.Offset = pContext->Eip;
	StackFrame.AddrPC.Mode = AddrModeFlat;
	StackFrame.AddrStack.Offset = pContext->Esp;
	StackFrame.AddrStack.Mode = AddrModeFlat;
	StackFrame.AddrFrame.Offset = pContext->Ebp;
	StackFrame.AddrFrame.Mode = AddrModeFlat;

	lprintf( _T("Call stack:\r\n") );

	if(verbose_flag)
		lprintf( _T("AddrPC     AddrReturn AddrFrame  AddrStack  Params\r\n") );

	while ( 1 )
	{
		BOOL bSuccess = FALSE;
		TCHAR szSymName[MAX_SYM_NAME_SIZE] = _T("");
		TCHAR szFileName[MAX_PATH] = _T("");
		DWORD dwLineNumber = 0;

		if(bSymInitialized)
		{
			if(!j_StackWalk(
					IMAGE_FILE_MACHINE_I386,
					hProcess,
					hThread,
					&StackFrame,
					pContext,
					NULL,
					j_SymFunctionTableAccess,
					j_SymGetModuleBase,
					NULL
				)
			)
				break;
		}
		else
		{
			if(!IntelStackWalk(
					IMAGE_FILE_MACHINE_I386,
					hProcess,
					hThread,
					&StackFrame,
					pContext,
					NULL,
					NULL,
					NULL,
					NULL
				)
			)
				break;
		}			
		
		// Basic sanity check to make sure  the frame is OK.  Bail if not.
		if ( 0 == StackFrame.AddrFrame.Offset ) 
			break;
		
		if(verbose_flag)
			lprintf(
				_T("%08lX   %08lX   %08lX   %08lX   %08lX   %08lX   %08lX   %08lX\r\n"),
				StackFrame.AddrPC.Offset,
				StackFrame.AddrReturn.Offset,
				StackFrame.AddrFrame.Offset, 
				StackFrame.AddrStack.Offset,
				StackFrame.Params[0],
				StackFrame.Params[1],
				StackFrame.Params[2],
				StackFrame.Params[3]
			);

		lprintf( _T("%08lX"), StackFrame.AddrPC.Offset);
		
		if((hModule = (HMODULE) GetModuleBase(hProcess, StackFrame.AddrPC.Offset)) && j_GetModuleFileNameEx(hProcess, hModule, szModule, sizeof(szModule)))
		{
			PMODULE_LIST_INFO pModuleListInfo;
			
			bfd *abfd = NULL;
			asymbol **syms = NULL;	// The symbol table.
			long symcount = 0;	// Number of symbols in `syms'.
			void *dhandle = NULL;
		
			lprintf( _T("  %s:%08lX"), GetBaseName(szModule), StackFrame.AddrPC.Offset);

			// Find the module from the module list
			assert(nModules);
			i = 0;
			while(i < nModules && (pProcessListInfo->dwProcessId > ModuleListInfo[i].dwProcessId || (pProcessListInfo->dwProcessId == ModuleListInfo[i].dwProcessId && (LPVOID)hModule > ModuleListInfo[i].lpBaseAddress)))
				++i;
			assert(ModuleListInfo[i].dwProcessId == pProcessListInfo->dwProcessId);
			assert(ModuleListInfo[i].lpBaseAddress == (LPVOID)hModule);
			assert(ModuleListInfo[i].dwProcessId == pProcessListInfo->dwProcessId && ModuleListInfo[i].lpBaseAddress == (LPVOID)hModule);
			pModuleListInfo = &ModuleListInfo[i];
			
			if(pModuleListInfo->abfd)
			{
				abfd = pModuleListInfo->abfd;
				syms = pModuleListInfo->syms;
				symcount = pModuleListInfo->symcount;
				dhandle = pModuleListInfo->dhandle;
			}
			else
			{
				if((abfd = pModuleListInfo->abfd = bfd_openr (szModule, NULL)))
					if(bfd_check_format(abfd, bfd_object))
					{
						bfd_vma adjust_section_vma = 0;
			
						/* If we are adjusting section VMA's, change them all now.  Changing
						the BFD information is a hack.  However, we must do it, or
						bfd_find_nearest_line will not do the right thing.  */
						if ((adjust_section_vma = (bfd_vma) hModule - pe_data(abfd)->pe_opthdr.ImageBase))
						{
							asection *s;
						
							for (s = abfd->sections; s != NULL; s = s->next)
							{
								s->vma += adjust_section_vma;
								s->lma += adjust_section_vma;
							}
						}
						
						if(bfd_get_file_flags(abfd) & HAS_SYMS)
						{
							/* Read in the symbol table.  */
							slurp_symtab(abfd, (asymbol ***) &pModuleListInfo->syms, &pModuleListInfo->symcount);
							
							if((syms = pModuleListInfo->syms) && (symcount = pModuleListInfo->symcount))
								dhandle = pModuleListInfo->dhandle = read_debugging_info (abfd, syms, symcount);
						}
					}
			}
			
			if(!bSuccess && abfd && syms && symcount)
			{
				if((bSuccess = BfdGetSymFromAddr(abfd, syms, symcount, hProcess, StackFrame.AddrPC.Offset, szSymName, MAX_SYM_NAME_SIZE)))
				{
					TCHAR szDemSymName[512];

					BfdDemangleSymName(szSymName, szDemSymName, 512);
					
					lprintf( _T("  %s"), szDemSymName);
					
					if(BfdGetLineFromAddr(abfd, syms, symcount, hProcess, StackFrame.AddrPC.Offset, szFileName, MAX_PATH, &dwLineNumber))
						lprintf( _T("  %s:%ld"), GetBaseName(szFileName), dwLineNumber);
					
					if (dhandle)
					{
						lprintf(_T("\r\n"));
						print_function_info(abfd, syms, symcount, dhandle, hProcess, szSymName, StackFrame.AddrFrame.Offset);
					}
				}
				else if(dhandle && (bSuccess = get_line_from_addr(abfd, syms, symcount, dhandle, StackFrame.AddrPC.Offset, szFileName, MAX_PATH, (unsigned *) &dwLineNumber)))
					lprintf( _T("  %s:%ld"), GetBaseName(szFileName), dwLineNumber);

			}
			
			if(!bSuccess && bSymInitialized)
				if((bSuccess = ImagehlpGetSymFromAddr(hProcess, StackFrame.AddrPC.Offset, szSymName, MAX_SYM_NAME_SIZE)))
				{
					ImagehlpDemangleSymName(szSymName, szSymName, MAX_SYM_NAME_SIZE);
				
					lprintf( _T("  %s"), szSymName);
					
					if(ImagehlpGetLineFromAddr(hProcess, StackFrame.AddrPC.Offset, szFileName, MAX_PATH, &dwLineNumber))
						lprintf( _T("  %s:%ld"), GetBaseName(szFileName), dwLineNumber);
				}

			if(!bSuccess && (bSuccess = PEGetSymFromAddr(hProcess, StackFrame.AddrPC.Offset, szSymName, MAX_SYM_NAME_SIZE)))
				lprintf( _T("  %s"), szSymName);
		}

		lprintf(_T("\r\n"));
		
		if(bSuccess && DumpSource(szFileName, dwLineNumber))
			lprintf(_T("\r\n"));
	}
	
	if(bSymInitialized)
	{
		if(!j_SymCleanup(hProcess))
			assert(0);
		
		bSymInitialized = FALSE;
	}
	
	return TRUE;
}
