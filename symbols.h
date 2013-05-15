/* This is a Cfunctions (version 0.28) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from 'http://www.lemoda.net/cfunctions/'. */

/* This file was generated with:
'cfunctions -i symbols.c' */
#ifndef CFH_SYMBOLS_H
#define CFH_SYMBOLS_H

/* From 'symbols.c': */
#define MAX_SYM_NAME_SIZE	4096
DWORD GetModuleBase (HANDLE hProcess , DWORD dwAddress );
#include <dbghelp.h>
extern BOOL bSymInitialized;
BOOL ImagehlpGetSymFromAddr (HANDLE hProcess , DWORD64 dwAddress , LPTSTR lpSymName , DWORD nSize );
BOOL ImagehlpGetLineFromAddr (HANDLE hProcess , DWORD64 dwAddress , LPTSTR lpFileName , DWORD nSize , LPDWORD lpLineNumber );
BOOL PEGetSymFromAddr (HANDLE hProcess , DWORD dwAddress , LPTSTR lpSymName , DWORD nSize );

#endif /* CFH_SYMBOLS_H */
