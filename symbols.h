/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i symbols.c' */
#ifndef CFH_SYMBOLS_H
#define CFH_SYMBOLS_H

/* From `symbols.c': */
#define MAX_SYM_NAME_SIZE	4096
#include <bfd.h>
BOOL BfdDemangleSymName (LPCTSTR lpName , LPTSTR lpDemangledName , DWORD nSize );
BOOL BfdGetSymFromAddr (bfd *abfd , asymbol **syms , long symcount , HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL BfdGetLineFromAddr (bfd *abfd , asymbol **syms , long symcount , HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
#include <imagehlp.h>
extern BOOL bSymInitialized;
BOOL WINAPI j_SymInitialize (HANDLE hProcess , PSTR UserSearchPath , BOOL fInvadeProcess );
BOOL WINAPI j_SymCleanup (HANDLE hProcess );
PVOID WINAPI j_SymFunctionTableAccess (HANDLE hProcess , DWORD AddrBase );
DWORD WINAPI j_SymGetModuleBase (HANDLE hProcess , DWORD dwAddr );
BOOL WINAPI j_StackWalk (DWORD MachineType , HANDLE hProcess , HANDLE hThread , LPSTACKFRAME StackFrame , PVOID ContextRecord , PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine , PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine , PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine , PTRANSLATE_ADDRESS_ROUTINE TranslateAddress );
BOOL WINAPI j_SymGetSymFromAddr (HANDLE hProcess , DWORD Address , PDWORD Displacement , PIMAGEHLP_SYMBOL Symbol );
BOOL WINAPI j_SymGetLineFromAddr (HANDLE hProcess , DWORD dwAddr , PDWORD pdwDisplacement , PIMAGEHLP_LINE Line );
BOOL ImagehlpDemangleSymName (LPCTSTR lpName , LPTSTR lpDemangledName , DWORD nSize );
BOOL ImagehlpGetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL ImagehlpGetLineFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
BOOL PEGetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL GetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL GetLineFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
BOOL WINAPI IntelStackWalk (DWORD MachineType , HANDLE hProcess , HANDLE hThread , LPSTACKFRAME StackFrame , PCONTEXT ContextRecord , PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine , PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine , PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine , PTRANSLATE_ADDRESS_ROUTINE TranslateAddress );
BOOL StackBackTrace (HANDLE hProcess , HANDLE hThread , PCONTEXT pContext );

#endif /* CFH_SYMBOLS_H */
