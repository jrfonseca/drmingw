#ifndef PEHELP_H
#define PEHELP_H

PIMAGE_NT_HEADERS
PEImageNtHeader(HANDLE hProcess, HMODULE hModule);

DWORD
PEGetImageBase(HANDLE hProcess, HMODULE hModule);

BOOL
PEGetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize);

#endif // PEHELP_H
