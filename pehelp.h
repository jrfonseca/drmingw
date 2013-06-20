#ifndef PEHELP_H
#define PEHELP_H

DWORD64 WINAPI
GetModuleBase64(HANDLE hProcess, DWORD64 dwAddress);

BOOL CALLBACK
ReadProcessMemory64(HANDLE hProcess, DWORD64 lpBaseAddress, PVOID lpBuffer, DWORD nSize, PDWORD lpNumberOfBytesRead);

DWORD64
PEImageNtHeader(HANDLE hProcess, DWORD64 hModule);

DWORD64
PEGetImageBase(HANDLE hProcess, DWORD64 hModule);

BOOL
PEGetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize);

#endif // PEHELP_H
