/* This is a Cfunctions (version 0.28) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from 'http://www.lemoda.net/cfunctions/'. */

/* This file was generated with:
'cfunctions -i symbols.c' */
#ifndef CFH_SYMBOLS_H
#define CFH_SYMBOLS_H

/* From 'symbols.c': */
#define MAX_SYM_NAME_SIZE	4096
#include <bfd.h>
DWORD GetModuleBase (HANDLE hProcess , DWORD dwAddress );
BOOL BfdUnDecorateSymbolName (PCTSTR DecoratedName , PTSTR UnDecoratedName , DWORD UndecoratedLength , DWORD Flags );
BOOL BfdGetSymFromAddr (bfd *abfd , asymbol **syms , long symcount , HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL BfdGetLineFromAddr (bfd *abfd , asymbol **syms , long symcount , HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
#include <dbghelp.h>
extern BOOL bSymInitialized;
BOOL WINAPI j_SymInitialize (HANDLE hProcess , PSTR UserSearchPath , BOOL fInvadeProcess );
BOOL WINAPI j_SymCleanup (HANDLE hProcess );
DWORD WINAPI j_SymSetOptions (DWORD SymOptions );
BOOL WINAPI j_SymGetSymFromAddr64 (HANDLE hProcess , DWORD64 Address , PDWORD64 Displacement , PIMAGEHLP_SYMBOL64 Symbol );
BOOL WINAPI j_SymGetLineFromAddr64 (HANDLE hProcess , DWORD64 dwAddr , PDWORD pdwDisplacement , PIMAGEHLP_LINE64 Line );
BOOL ImagehlpUnDecorateSymbolName (PCTSTR DecoratedName , PTSTR UnDecoratedName , DWORD UndecoratedLength , DWORD Flags );
BOOL ImagehlpGetSymFromAddr (HANDLE hProcess , DWORD64 dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL ImagehlpGetLineFromAddr (HANDLE hProcess , DWORD64 dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
DWORD PEGetImageBase (HANDLE hProcess , HMODULE hModule );
BOOL PEGetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
bfd * BfdOpen (LPCSTR szModule , HANDLE hProcess , HMODULE hModule , asymbol ***syms , long *symcount );
BOOL GetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL GetLineFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );

#endif /* CFH_SYMBOLS_H */
