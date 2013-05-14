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
BOOL BfdDemangleSymName (LPCTSTR lpName , LPTSTR lpDemangledName , DWORD nSize );
BOOL BfdGetSymFromAddr (bfd *abfd , asymbol **syms , long symcount , HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL BfdGetLineFromAddr (bfd *abfd , asymbol **syms , long symcount , HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
#include <dbghelp.h>
extern BOOL bSymInitialized;
BOOL WINAPI j_SymInitialize (HANDLE hProcess , PSTR UserSearchPath , BOOL fInvadeProcess );
BOOL WINAPI j_SymCleanup (HANDLE hProcess );
PVOID WINAPI j_SymFunctionTableAccess64 (HANDLE hProcess , DWORD64 AddrBase );
DWORD64 WINAPI j_SymGetModuleBase64 (HANDLE hProcess , DWORD64 dwAddr );
BOOL WINAPI j_StackWalk64 (DWORD MachineType , HANDLE hProcess , HANDLE hThread , LPSTACKFRAME64 StackFrame , PVOID ContextRecord , PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine , PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine , PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine , PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );
BOOL WINAPI j_SymGetSymFromAddr64 (HANDLE hProcess , DWORD64 Address , PDWORD64 Displacement , PIMAGEHLP_SYMBOL64 Symbol );
BOOL WINAPI j_SymGetLineFromAddr64 (HANDLE hProcess , DWORD64 dwAddr , PDWORD pdwDisplacement , PIMAGEHLP_LINE64 Line );
BOOL ImagehlpDemangleSymName (LPCTSTR lpName , LPTSTR lpDemangledName , DWORD nSize );
BOOL ImagehlpGetSymFromAddr (HANDLE hProcess , DWORD64 dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL ImagehlpGetLineFromAddr (HANDLE hProcess , DWORD64 dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
BOOL PEGetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL GetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL GetLineFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
BOOL WINAPI IntelStackWalk (DWORD MachineType , HANDLE hProcess , HANDLE hThread , LPSTACKFRAME64 StackFrame , CONTEXT *ContextRecord , PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine , PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine , PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine , PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );
BOOL StackBackTrace (HANDLE hProcess , HANDLE hThread , PCONTEXT pContext );

#endif /* CFH_SYMBOLS_H */
