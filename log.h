/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i log.c' */
#ifndef CFH_LOG_H
#define CFH_LOG_H

/* From `log.c': */
void lflush (void);
int __cdecl lprintf (const TCHAR * format , ... );
BOOL LogException (DEBUG_EVENT DebugEvent );
BOOL DumpSource (LPCTSTR lpFileName , DWORD dwLineNumber );

#endif /* CFH_LOG_H */
