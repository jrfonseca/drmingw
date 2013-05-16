#include <windows.h>

#include "misc.h"
#include "pehelp.h"


PIMAGE_NT_HEADERS
PEImageNtHeader(HANDLE hProcess, HMODULE hModule)
{
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS pNtHeaders;
	LONG e_lfanew;

	// Point to the DOS header in memory
	pDosHeader = (PIMAGE_DOS_HEADER)hModule;

	// From the DOS header, find the NT (PE) header
	if(!ReadProcessMemory(hProcess, &pDosHeader->e_lfanew, &e_lfanew, sizeof e_lfanew, NULL))
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

	if(!ReadProcessMemory(hProcess, pNtHeaders, &NtHeaders, sizeof NtHeaders, NULL))
		return FALSE;

	return NtHeaders.OptionalHeader.ImageBase;
}

BOOL
PEGetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	HMODULE hModule;
	PIMAGE_NT_HEADERS pNtHeaders;
	IMAGE_NT_HEADERS NtHeaders;
	PIMAGE_SECTION_HEADER pSection;
	DWORD64 dwNearestAddress = 0;
	DWORD dwNearestName;
	int i;

	if(!(hModule = (HMODULE) GetModuleBase(hProcess, dwAddress)))
		return FALSE;

	if(!(pNtHeaders = PEImageNtHeader(hProcess, hModule)))
		return FALSE;

	if(!ReadProcessMemory(hProcess, pNtHeaders, &NtHeaders, sizeof(IMAGE_NT_HEADERS), NULL))
		return FALSE;

	pSection = (PIMAGE_SECTION_HEADER) ((DWORD64)pNtHeaders + sizeof(DWORD64) + sizeof(IMAGE_FILE_HEADER) + NtHeaders.FileHeader.SizeOfOptionalHeader);

	if (0)
		OutputDebug("Exported symbols:\r\n");

	// Look for export section
	for (i = 0; i < NtHeaders.FileHeader.NumberOfSections; i++, pSection++)
	{
		IMAGE_SECTION_HEADER Section;
		DWORD64 pExportDir = 0;
		BYTE ExportSectionName[IMAGE_SIZEOF_SHORT_NAME] = {'.', 'e', 'd', 'a', 't', 'a', '\0', '\0'};

		if(!ReadProcessMemory(hProcess, pSection, &Section, sizeof Section, NULL))
			return FALSE;

		if(memcmp(Section.Name, ExportSectionName, IMAGE_SIZEOF_SHORT_NAME) == 0)
			pExportDir = Section.VirtualAddress;
		else if ((NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress >= Section.VirtualAddress) && (NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress < (Section.VirtualAddress + Section.SizeOfRawData)))
			pExportDir = NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

		if(pExportDir)
		{
			IMAGE_EXPORT_DIRECTORY ExportDir;

			if(!ReadProcessMemory(hProcess, (PVOID)((DWORD64)hModule + (DWORD64)pExportDir), &ExportDir, sizeof ExportDir, NULL))
				return FALSE;

			{
				PDWORD *AddressOfFunctions = alloca(ExportDir.NumberOfFunctions*sizeof(PDWORD));
				int j;

				if(!ReadProcessMemory(hProcess, (PVOID)((DWORD64)hModule + (DWORD64)ExportDir.AddressOfFunctions), AddressOfFunctions, ExportDir.NumberOfFunctions*sizeof(PDWORD), NULL))
						return FALSE;

				for(j = 0; j < ExportDir.NumberOfNames; ++j)
				{
					DWORD64 pFunction = (DWORD64)hModule + (DWORD64)AddressOfFunctions[j];

					if(pFunction <= dwAddress && pFunction > dwNearestAddress)
					{
						dwNearestAddress = pFunction;

						if(!ReadProcessMemory(hProcess, (PVOID)((DWORD64)hModule + ExportDir.AddressOfNames + j*sizeof(DWORD)), &dwNearestName, sizeof(dwNearestName), NULL))
							return FALSE;
					}

					if (0)
					{
						DWORD dwName;
						char szName[256];

						if(ReadProcessMemory(hProcess, (PVOID)((DWORD64)hModule + ExportDir.AddressOfNames * j*sizeof(DWORD)), &dwName, sizeof(dwName), NULL))
							if(ReadProcessMemory(hProcess, (PVOID)((DWORD64)hModule + dwName), szName, sizeof(szName), NULL))
								OutputDebug("\t%08I64x\t%s\r\n", pFunction, szName);
					}
				}
			}
		}
    }

	if(!dwNearestAddress)
		return FALSE;

	if(!ReadProcessMemory(hProcess, (PVOID)((DWORD64)hModule + dwNearestName), lpSymName, nSize, NULL))
		return FALSE;
	lpSymName[nSize - 1] = 0;

	return TRUE;
}
