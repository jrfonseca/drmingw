/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i module.c' */
#ifndef CFH_MODULE_H
#define CFH_MODULE_H

/* From `module.c': */
DWORD GetModuleBase (HANDLE hProcess , DWORD dwAddress );
DWORD j_GetModuleBaseName (HANDLE hProcess , HMODULE hModule , LPTSTR lpBaseName , DWORD nSize );
DWORD j_GetModuleFileNameEx (HANDLE hProcess , HMODULE hModule , LPTSTR lpFilename , DWORD nSize );
BOOL GetVirtualAddress (HANDLE hProcess , DWORD dwAddress , LPDWORD lpOffset );

#endif /* CFH_MODULE_H */
