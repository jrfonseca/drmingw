/* symbols.c
 *
 *
 * Jose Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <psapi.h>

#include "misc.h"
#include "symbols.h"

#ifdef HEADER
#define MAX_SYM_NAME_SIZE	4096

#include <bfd.h>
#endif /* HEADER */

#include <libiberty.h>
#include <demangle.h>

// The GetModuleBase function retrieves the base address of the module that contains the specified address. 
DWORD GetModuleBase(HANDLE hProcess, DWORD dwAddress)
{
	MEMORY_BASIC_INFORMATION Buffer;
	
	return VirtualQueryEx(hProcess, (LPCVOID) dwAddress, &Buffer, sizeof(Buffer)) ? (DWORD) Buffer.AllocationBase : 0;
}


// Read in the symbol table.
static bfd_boolean
slurp_symtab (bfd *abfd, asymbol ***syms, long *symcount)
{
	unsigned int size;

	if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
		return FALSE;

	*symcount = bfd_read_minisymbols (abfd, FALSE, (void *) syms, &size);
	if (*symcount == 0)
		*symcount = bfd_read_minisymbols (abfd, TRUE /* dynamic */, (void *) syms, &size);

	if (*symcount < 0)
		return FALSE;

	return TRUE;
}

// This stucture is used to pass information between translate_addresses and find_address_in_section.
struct find_handle
{
	asymbol **syms;
	bfd_vma pc;
	const char *filename;
	const char *functionname;
	unsigned int line;
	bfd_boolean found;
};

// Look for an address in a section.  This is called via  bfd_map_over_sections. 
static void find_address_in_section (bfd *abfd, asection *section, void *data)
{
	struct find_handle *info = (struct find_handle *) data;
	bfd_vma vma;
	bfd_size_type size;

	if (info->found)
		return;

	if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
		return;
	
	vma = bfd_get_section_vma (abfd, section);
	if (info->pc < vma)
		return;

	size = bfd_get_section_size (section);
	if (info->pc >= vma + size)
		return;

	info->found = bfd_find_nearest_line (abfd, section, info->syms, info->pc - vma, 
                                             &info->filename, &info->functionname, &info->line);
}

BOOL BfdUnDecorateSymbolName(PCTSTR DecoratedName, PTSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags)
{
	char *res;
	
	assert(DecoratedName != NULL);
	
	if((res = cplus_demangle(DecoratedName, DMGL_ANSI /*| DMGL_PARAMS*/)) == NULL)
	{
		lstrcpyn(UnDecoratedName, DecoratedName, UndecoratedLength);
		return FALSE;
	}
	else
	{
		lstrcpyn(UnDecoratedName, res, UndecoratedLength);
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
	
	assert(bfd_get_file_flags (abfd) & HAS_SYMS);
	assert(symcount);

	info.pc = dwAddress;
	info.syms = syms;
	info.found = FALSE;

	bfd_map_over_sections (abfd, find_address_in_section, &info);
	if (info.found == FALSE || info.line == 0)
	{
		if(verbose_flag)
			OutputDebug("%s: %s\r\n", bfd_get_filename (abfd), "No symbol found");
		return FALSE;
	}

	assert(lpSymName);
	
	if(info.functionname == NULL || *info.functionname == '\0')
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
	
	assert(bfd_get_file_flags (abfd) & HAS_SYMS);
	assert(symcount);

	info.pc = dwAddress;
	info.syms = syms;

	info.found = FALSE;

	bfd_map_over_sections (abfd, find_address_in_section, &info);
	if (info.found == FALSE || info.line == 0)
	{
		if(verbose_flag)
			OutputDebug("%s: %s\r\n", bfd_get_filename (abfd), "No symbol found");
		return FALSE;
	}

	assert(lpFileName && lpLineNumber);

	lstrcpyn(lpFileName, info.filename, nSize);
	*lpLineNumber = info.line;

	return TRUE;
}


#ifdef HEADER
#include <dbghelp.h>
#endif /* HEADER */

BOOL bSymInitialized = FALSE;

static HMODULE hModule_Imagehlp = NULL;

typedef BOOL (WINAPI *PFNSYMINITIALIZE)(HANDLE, LPSTR, BOOL);
static PFNSYMINITIALIZE pfnSymInitialize = NULL;

BOOL WINAPI j_SymInitialize(HANDLE hProcess, PSTR UserSearchPath, BOOL fInvadeProcess)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
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
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymCleanup || (pfnSymCleanup = (PFNSYMCLEANUP) GetProcAddress(hModule_Imagehlp, "SymCleanup")))
	)
		return pfnSymCleanup(hProcess);
	else
		return FALSE;
}

typedef DWORD (WINAPI *PFNSYMSETOPTIONS)(DWORD);
static PFNSYMSETOPTIONS pfnSymSetOptions = NULL;

DWORD WINAPI j_SymSetOptions(DWORD SymOptions)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymSetOptions || (pfnSymSetOptions = (PFNSYMSETOPTIONS) GetProcAddress(hModule_Imagehlp, "SymSetOptions")))
	)
		return pfnSymSetOptions(SymOptions);
	else
		return FALSE;
}

typedef BOOL (WINAPI *PFNSYMUNDNAME64)(PIMAGEHLP_SYMBOL64, PSTR, DWORD);
static PFNSYMUNDNAME64 pfnSymUnDName64 = NULL;

static
BOOL WINAPI j_SymUnDName64(PIMAGEHLP_SYMBOL64 Symbol, PSTR UnDecName, DWORD UnDecNameLength)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymUnDName64 || (pfnSymUnDName64 = (PFNSYMUNDNAME64) GetProcAddress(hModule_Imagehlp, "SymUnDName64")))
	)
		return pfnSymUnDName64(Symbol, UnDecName, UnDecNameLength);
	else
		return FALSE;
}

typedef PFUNCTION_TABLE_ACCESS_ROUTINE64 PFNSYMFUNCTIONTABLEACCESS64;
static PFNSYMFUNCTIONTABLEACCESS64 pfnSymFunctionTableAccess = NULL;

PVOID WINAPI j_SymFunctionTableAccess64(HANDLE hProcess, DWORD64 AddrBase)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymFunctionTableAccess || (pfnSymFunctionTableAccess = (PFNSYMFUNCTIONTABLEACCESS64) GetProcAddress(hModule_Imagehlp, "SymFunctionTableAccess64")))
	)
		return pfnSymFunctionTableAccess(hProcess, AddrBase);
	else
		return NULL;
}

typedef PGET_MODULE_BASE_ROUTINE64 PFNSYMGETMODULEBASE64;
static PFNSYMGETMODULEBASE64 pfnSymGetModuleBase64 = NULL;

DWORD64 WINAPI j_SymGetModuleBase64(HANDLE hProcess, DWORD64 dwAddr)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymGetModuleBase64 || (pfnSymGetModuleBase64 = (PFNSYMGETMODULEBASE64) GetProcAddress(hModule_Imagehlp, "SymGetModuleBase64")))
	)
		return pfnSymGetModuleBase64(hProcess, dwAddr);
	else
		return 0;
}

typedef BOOL (WINAPI *PFNSTACKWALK64)(
	DWORD MachineType,
	HANDLE hProcess,
	HANDLE hThread,
	LPSTACKFRAME64 StackFrame,
	PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
);
static PFNSTACKWALK64 pfnStackWalk64 = NULL;

BOOL WINAPI j_StackWalk64(
	DWORD MachineType, 
	HANDLE hProcess, 
	HANDLE hThread, 
	LPSTACKFRAME64 StackFrame,
	PVOID ContextRecord, 
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnStackWalk64 || (pfnStackWalk64 = (PFNSTACKWALK64) GetProcAddress(hModule_Imagehlp, "StackWalk64")))
	)
		return pfnStackWalk64(
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

typedef BOOL (WINAPI *PFNSYMGETSYMFROMADDR64)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
static PFNSYMGETSYMFROMADDR64 pfnSymGetSymFromAddr64 = NULL;

BOOL WINAPI j_SymGetSymFromAddr64(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PIMAGEHLP_SYMBOL64 Symbol)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymGetSymFromAddr64 || (pfnSymGetSymFromAddr64 = (PFNSYMGETSYMFROMADDR64) GetProcAddress(hModule_Imagehlp, "SymGetSymFromAddr64")))
	)
		return pfnSymGetSymFromAddr64(hProcess, Address, Displacement, Symbol);
	else
		return FALSE;
}

typedef BOOL (WINAPI *PFNSYMGETLINEFROMADDR64)(HANDLE, DWORD64, LPDWORD, PIMAGEHLP_LINE64);
static PFNSYMGETLINEFROMADDR64 pfnSymGetLineFromAddr64 = NULL;

BOOL WINAPI j_SymGetLineFromAddr64(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line)
{
	if(
		(hModule_Imagehlp || (hModule_Imagehlp = LoadLibrary(_T("DBGHELP")))) &&
		(pfnSymGetLineFromAddr64 || (pfnSymGetLineFromAddr64 = (PFNSYMGETLINEFROMADDR64) GetProcAddress(hModule_Imagehlp, "SymGetLineFromAddr64")))
	)
		return pfnSymGetLineFromAddr64(hProcess, dwAddr, pdwDisplacement, Line);
	else
		return FALSE;
}

BOOL ImagehlpUnDecorateSymbolName(PCTSTR DecoratedName, PTSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags)
{
	BYTE symbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 512];
	PIMAGEHLP_SYMBOL64 pSymbol = (PIMAGEHLP_SYMBOL64) symbolBuffer;

	memset( symbolBuffer, 0, sizeof(symbolBuffer) );
	
	pSymbol->SizeOfStruct = sizeof(symbolBuffer);
	pSymbol->MaxNameLength = 512;

	lstrcpyn(pSymbol->Name, DecoratedName, pSymbol->MaxNameLength);

	if(!j_SymUnDName64(pSymbol, UnDecoratedName, UndecoratedLength))
		return FALSE;
	
	return TRUE;
}

BOOL ImagehlpGetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	// IMAGEHLP is wacky, and requires you to pass in a pointer to a
	// IMAGEHLP_SYMBOL64 structure.  The problem is that this structure is
	// variable length.  That is, you determine how big the structure is
	// at runtime.  This means that you can't use sizeof(struct).
	// So...make a buffer that's big enough, and make a pointer
	// to the buffer.  We also need to initialize not one, but TWO
	// members of the structure before it can be used.
	
	BYTE symbolBuffer[sizeof(IMAGEHLP_SYMBOL64) + 512];
	PIMAGEHLP_SYMBOL64 pSymbol = (PIMAGEHLP_SYMBOL64) symbolBuffer;
	DWORD64 dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

	pSymbol->SizeOfStruct = sizeof(symbolBuffer);
	pSymbol->MaxNameLength = 512;

	assert(bSymInitialized);
	
	if(!j_SymGetSymFromAddr64(hProcess, dwAddress, &dwDisplacement, pSymbol))
		return FALSE;

	lstrcpyn(lpSymName, pSymbol->Name, nSize);

	return TRUE;
}

BOOL ImagehlpGetLineFromAddr(HANDLE hProcess, DWORD64 dwAddress,  LPTSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
	IMAGEHLP_LINE64 Line;
	DWORD dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

	// Do the source and line lookup.
	memset(&Line, 0, sizeof Line);
	Line.SizeOfStruct = sizeof Line;
	
	assert(bSymInitialized);

#if 1
	{
		// The problem is that the symbol engine only finds those source
		//  line addresses (after the first lookup) that fall exactly on
		//  a zero displacement.  I will walk backwards 100 bytes to
		//  find the line and return the proper displacement.
		DWORD64 dwTempDisp = 0 ;
		while (dwTempDisp < 100 && !j_SymGetLineFromAddr64(hProcess, dwAddress - dwTempDisp, &dwDisplacement, &Line))
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

static PIMAGE_NT_HEADERS
PEImageNtHeader(HANDLE hProcess, HMODULE hModule)
{
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS pNtHeaders;
	LONG e_lfanew;
	
	// Point to the DOS header in memory
	pDosHeader = (PIMAGE_DOS_HEADER)hModule;
	
	// From the DOS header, find the NT (PE) header
	if(!ReadProcessMemory(hProcess, &pDosHeader->e_lfanew, &e_lfanew, sizeof(e_lfanew), NULL))
		return NULL;
	
	pNtHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)hModule + e_lfanew);

	return pNtHeaders;
}

DWORD 
PEGetImageBase(HANDLE hProcess, HMODULE hModule)
{
	PIMAGE_NT_HEADERS pNtHeaders;
	IMAGE_NT_HEADERS NtHeaders;

	if(!(pNtHeaders = PEImageNtHeader(hProcess, hModule)))
		return FALSE;

	if(!ReadProcessMemory(hProcess, pNtHeaders, &NtHeaders, sizeof(IMAGE_NT_HEADERS), NULL))
		return FALSE;

	return NtHeaders.OptionalHeader.ImageBase;
}
	
BOOL PEGetSymFromAddr(HANDLE hProcess, DWORD dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	HMODULE hModule;
	PIMAGE_NT_HEADERS pNtHeaders;
	IMAGE_NT_HEADERS NtHeaders;
	PIMAGE_SECTION_HEADER pSection;
	DWORD dwNearestAddress = 0, dwNearestName;
	int i;

	if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)))
		return FALSE;
	
	if(!(pNtHeaders = PEImageNtHeader(hProcess, hModule)))
		return FALSE;

	if(!ReadProcessMemory(hProcess, pNtHeaders, &NtHeaders, sizeof(IMAGE_NT_HEADERS), NULL))
		return FALSE;
	
	pSection = (PIMAGE_SECTION_HEADER) ((DWORD)pNtHeaders + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + NtHeaders.FileHeader.SizeOfOptionalHeader);

	if (0)
		OutputDebug("Exported symbols:\r\n");

	// Look for export section
	for (i = 0; i < NtHeaders.FileHeader.NumberOfSections; i++, pSection++)
	{
		IMAGE_SECTION_HEADER Section;
		PIMAGE_EXPORT_DIRECTORY pExportDir = NULL;
		BYTE ExportSectionName[IMAGE_SIZEOF_SHORT_NAME] = {'.', 'e', 'd', 'a', 't', 'a', '\0', '\0'};
		
		if(!ReadProcessMemory(hProcess, pSection, &Section, sizeof(IMAGE_SECTION_HEADER), NULL))
			return FALSE;
    	
		if(memcmp(Section.Name, ExportSectionName, IMAGE_SIZEOF_SHORT_NAME) == 0)
			pExportDir = (PIMAGE_EXPORT_DIRECTORY) Section.VirtualAddress;
		else if ((NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress >= Section.VirtualAddress) && (NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress < (Section.VirtualAddress + Section.SizeOfRawData)))
			pExportDir = (PIMAGE_EXPORT_DIRECTORY) NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

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
						
						if(!ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + ExportDir.AddressOfNames + j*sizeof(DWORD)), &dwNearestName, sizeof(dwNearestName), NULL))
							return FALSE;
							
						dwNearestName = (DWORD) hModule + dwNearestName;
					}
				
					if (0)
					{
						DWORD dwName;
						char szName[256];
						
						if(ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + ExportDir.AddressOfNames * j*sizeof(DWORD)), &dwName, sizeof(dwName), NULL))
							if(ReadProcessMemory(hProcess, (PVOID)((DWORD)hModule + dwName), szName, sizeof(szName), NULL))
								OutputDebug("\t%08x\t%s\r\n", pFunction, szName);
					}
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

static BOOL CALLBACK ReadProcessMemory64(
   HANDLE hProcess,
   DWORD64 lpBaseAddress,
   PVOID lpBuffer,
   DWORD nSize,
   PDWORD lpNumberOfBytesRead
) {
	DWORD NumberOfBytesRead = 0;
	BOOL bRet = ReadProcessMemory(hProcess, (LPCVOID)(UINT_PTR)lpBaseAddress, lpBuffer, nSize, &NumberOfBytesRead);
	if (lpNumberOfBytesRead) {
		*lpNumberOfBytesRead = NumberOfBytesRead;
	}
	return bRet;
}

bfd *
BfdOpen(LPCSTR szModule, HANDLE hProcess, HMODULE hModule, asymbol ***syms, long *symcount)
{
	bfd *abfd;
	bfd_vma adjust_section_vma;
	bfd_vma image_base_vma;

	abfd = bfd_openr (szModule, NULL);
	if(!abfd)
		goto no_bfd;

	if(!bfd_check_format(abfd, bfd_object))
	{
		if(verbose_flag)
			OutputDebug("\r\n%s: %s\r\n", szModule, "Bad format");
		goto bad_format;
	}

	if(!(bfd_get_file_flags(abfd) & HAS_SYMS))
	{
		if(verbose_flag)
			OutputDebug("\r\n%s: %s\r\n", szModule, "No symbols");
		goto no_symbols;
	}

#if 0
	image_base_vma = pe_data (abfd)->pe_opthdr.ImageBase;
#else
	image_base_vma = (bfd_vma) PEGetImageBase(hProcess, hModule);
#endif

	/* If we are adjusting section VMA's, change them all now.  Changing
	the BFD information is a hack.  However, we must do it, or
	bfd_find_nearest_line will not do the right thing.  */
	if ((adjust_section_vma = (bfd_vma) hModule - image_base_vma))
	{
		asection *s;
	
		if(verbose_flag)
			OutputDebug("\r\nadjusting sections from 0x%08lx to 0x%08lx, 0x%08lx\r\n", image_base_vma, hModule, adjust_section_vma);
	
		for (s = abfd->sections; s != NULL; s = s->next)
			bfd_set_section_vma(abfd, s, bfd_get_section_vma(abfd, s) + adjust_section_vma);
	}

	if(!slurp_symtab(abfd, syms, symcount))
		goto no_symbols;

	if(!*symcount)
		goto zero_symbols;

	return abfd;

zero_symbols:
	free(*syms);
no_symbols:
bad_format:
	bfd_close(abfd);
no_bfd:
	*syms = NULL;
	*symcount = 0;
	return NULL;
}

BOOL GetSymFromAddr(HANDLE hProcess, DWORD dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	BOOL bReturn = FALSE;
	
	{
		HMODULE hModule;
		
		TCHAR szModule[MAX_PATH]; 
		bfd_error_handler_type old_bfd_error_handler;
		bfd *abfd;
		asymbol **syms;
		long symcount;
	
		if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)) || !GetModuleFileNameEx(hProcess, hModule, szModule, sizeof(szModule)))
			return FALSE;

		old_bfd_error_handler = bfd_set_error_handler((bfd_error_handler_type) OutputDebug);
			
		if((abfd = BfdOpen(szModule, hProcess, hModule, &syms, &symcount)))
		{
			if((bReturn = BfdGetSymFromAddr(abfd, syms, symcount, hProcess, dwAddress, lpSymName, nSize)))
				BfdUnDecorateSymbolName(lpSymName, lpSymName, nSize, UNDNAME_COMPLETE);

			free(syms);
			bfd_close(abfd);
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
					OutputDebug("SymInitialize: %s\r\n", LastErrorMessage());
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
		asymbol **syms;
		long symcount;
	
		if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)) || !GetModuleFileNameEx(hProcess, hModule, szModule, sizeof(szModule)))
			return FALSE;

		old_bfd_error_handler = bfd_set_error_handler((bfd_error_handler_type) OutputDebug);
	
		if((abfd = BfdOpen (szModule, hProcess, hModule, &syms, &symcount)))
		{
			bReturn = BfdGetLineFromAddr(abfd, syms, symcount, hProcess, dwAddress, lpFileName, nSize, lpLineNumber);
			free(syms);
			bfd_close(abfd);
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
					OutputDebug("SymInitialize: %s\r\n", LastErrorMessage());
		}
	}
	
	return bReturn;
}	

BOOL WINAPI IntelStackWalk(
	DWORD MachineType, 
	HANDLE hProcess, 
	HANDLE hThread, 
	LPSTACKFRAME64 StackFrame,
	CONTEXT *ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
)
{
	assert(MachineType == IMAGE_FILE_MACHINE_I386);
	
	if(ReadMemoryRoutine == NULL)
		ReadMemoryRoutine = &ReadProcessMemory64;
	
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
		if(!ReadMemoryRoutine(hProcess, StackFrame->AddrFrame.Offset + sizeof (DWORD), &StackFrame->AddrReturn.Offset, sizeof(DWORD), NULL))
			return FALSE;
	}
	else
	{
		StackFrame->AddrPC.Offset = StackFrame->AddrReturn.Offset;
		//AddrStack = AddrFrame + 2*sizeof(DWORD);
		if(!ReadMemoryRoutine(hProcess, StackFrame->AddrFrame.Offset, &StackFrame->AddrFrame.Offset, sizeof(DWORD), NULL))
			return FALSE;
		if(!ReadMemoryRoutine(hProcess, StackFrame->AddrFrame.Offset + sizeof(DWORD), &StackFrame->AddrReturn.Offset, sizeof(DWORD), NULL))
			return FALSE;
	}

	ReadMemoryRoutine(hProcess, StackFrame->AddrFrame.Offset + 2*sizeof(DWORD), StackFrame->Params, sizeof(StackFrame->Params), NULL);
	
	return TRUE;	
}

