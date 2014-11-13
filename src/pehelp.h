#ifndef PEHELP_H
#define PEHELP_H

DWORD64 WINAPI
GetModuleBase64(HANDLE hProcess, DWORD64 dwAddress);

DWORD64
PEGetImageBase(HANDLE hProcess, DWORD64 hModule);

#endif // PEHELP_H
