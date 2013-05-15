/* This is a Cfunctions (version 0.28) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from 'http://www.lemoda.net/cfunctions/'. */

/* This file was generated with:
'cfunctions -i log.c' */
#ifndef CFH_LOG_H
#define CFH_LOG_H

/* From 'log.c': */
void lflush (void);
int __cdecl lprintf (const TCHAR * format , ... );
LPTSTR GetBaseName (LPTSTR lpFileName );
BOOL LogException (DEBUG_EVENT DebugEvent );
BOOL DumpSource (LPCTSTR lpFileName , DWORD dwLineNumber );
BOOL StackBackTrace (HANDLE hProcess , HANDLE hThread , PCONTEXT pContext );

#endif /* CFH_LOG_H */
