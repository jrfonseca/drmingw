#include <stddef.h>

#include <windows.h>

#include "misc.h"
#include "pehelp.h"


// The GetModuleBase function retrieves the base address of the module that contains the specified address.
DWORD64
GetModuleBase(HANDLE hProcess, DWORD64 dwAddress)
{
	MEMORY_BASIC_INFORMATION Buffer;

	if (VirtualQueryEx(hProcess, (LPCVOID) (INT_PTR) dwAddress, &Buffer, sizeof Buffer) == 0) {
		return 0;
	}

	return (DWORD64) (INT_PTR) Buffer.AllocationBase;
}

BOOL CALLBACK
ReadProcessMemory64(HANDLE hProcess, DWORD64 lpBaseAddress, PVOID lpBuffer, DWORD nSize, PDWORD lpNumberOfBytesRead)
{
	SIZE_T NumberOfBytesRead = 0;
	BOOL bRet = ReadProcessMemory(hProcess, (LPCVOID)(UINT_PTR)lpBaseAddress, lpBuffer, nSize, &NumberOfBytesRead);
	if (lpNumberOfBytesRead) {
		*lpNumberOfBytesRead = NumberOfBytesRead;
	}
	return bRet;
}

DWORD64
PEImageNtHeader(HANDLE hProcess, DWORD64 hModule)
{
	LONG e_lfanew;

	// From the DOS header, find the NT (PE) header
	if (!ReadProcessMemory64(hProcess, hModule + offsetof(IMAGE_DOS_HEADER, e_lfanew), &e_lfanew, sizeof e_lfanew, NULL))
		return 0;

	return hModule + e_lfanew;
}

DWORD64
PEGetImageBase(HANDLE hProcess, DWORD64 hModule)
{
	DWORD64 pNtHeaders;
	IMAGE_NT_HEADERS NtHeaders;

	if(!(pNtHeaders = PEImageNtHeader(hProcess, hModule)))
		return FALSE;

	if(!ReadProcessMemory64(hProcess, pNtHeaders, &NtHeaders, sizeof NtHeaders, NULL))
		return FALSE;

	return NtHeaders.OptionalHeader.ImageBase;
}

BOOL
PEGetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	DWORD64 hModule;
	DWORD64 pNtHeaders;
	IMAGE_NT_HEADERS NtHeaders;
	DWORD64 pSection;
	DWORD64 dwNearestAddress = 0;
	DWORD dwNearestName;
	int i;

	if(!(hModule = GetModuleBase(hProcess, dwAddress)))
		return FALSE;

	if(!(pNtHeaders = PEImageNtHeader(hProcess, hModule)))
		return FALSE;

	if(!ReadProcessMemory64(hProcess, pNtHeaders, &NtHeaders, sizeof(IMAGE_NT_HEADERS), NULL))
		return FALSE;

	pSection = pNtHeaders + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + NtHeaders.FileHeader.SizeOfOptionalHeader;

	if (0)
		OutputDebug("Exported symbols:\r\n");

	// Look for export section
	for (i = 0; i < NtHeaders.FileHeader.NumberOfSections; i++, pSection += sizeof(IMAGE_SECTION_HEADER))
	{
		IMAGE_SECTION_HEADER Section;
		DWORD64 pExportDir = 0;
		BYTE ExportSectionName[IMAGE_SIZEOF_SHORT_NAME] = {'.', 'e', 'd', 'a', 't', 'a', '\0', '\0'};

		if(!ReadProcessMemory64(hProcess, pSection, &Section, sizeof Section, NULL))
			return FALSE;

		if(memcmp(Section.Name, ExportSectionName, IMAGE_SIZEOF_SHORT_NAME) == 0)
			pExportDir = Section.VirtualAddress;
		else if ((NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress >= Section.VirtualAddress) && (NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress < (Section.VirtualAddress + Section.SizeOfRawData)))
			pExportDir = NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

		if(pExportDir)
		{
			IMAGE_EXPORT_DIRECTORY ExportDir;

			if(!ReadProcessMemory64(hProcess, hModule + pExportDir, &ExportDir, sizeof ExportDir, NULL))
				return FALSE;

			{
				DWORD *AddressOfFunctions = alloca(ExportDir.NumberOfFunctions * sizeof *AddressOfFunctions);
				int j;

				if(!ReadProcessMemory64(hProcess, hModule + ExportDir.AddressOfFunctions, AddressOfFunctions, ExportDir.NumberOfFunctions * sizeof *AddressOfFunctions, NULL))
						return FALSE;

				for(j = 0; j < ExportDir.NumberOfNames; ++j)
				{
					DWORD64 pFunction = hModule + AddressOfFunctions[j];

					if(pFunction <= dwAddress && pFunction > dwNearestAddress)
					{
						dwNearestAddress = pFunction;

						if(!ReadProcessMemory64(hProcess, hModule + ExportDir.AddressOfNames + j*sizeof(DWORD), &dwNearestName, sizeof dwNearestName, NULL))
							return FALSE;
					}

					if (0)
					{
						DWORD dwName;
						char szName[256];

						if(ReadProcessMemory64(hProcess, hModule + ExportDir.AddressOfNames * j*sizeof(DWORD), &dwName, sizeof dwName, NULL))
							if(ReadProcessMemory64(hProcess, hModule + dwName, szName, sizeof szName, NULL))
								OutputDebug("\t%08I64x\t%s\r\n", pFunction, szName);
					}
				}
			}
		}
    }

	if(!dwNearestAddress)
		return FALSE;

	if(!ReadProcessMemory64(hProcess, hModule + dwNearestName, lpSymName, nSize, NULL))
		return FALSE;
	lpSymName[nSize - 1] = 0;

	return TRUE;
}
