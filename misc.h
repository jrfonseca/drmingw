/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i misc.c' */
#ifndef CFH_MISC_H
#define CFH_MISC_H

#include <stdio.h>

/* From `misc.c': */
extern int verbose_flag;

static inline void
OutputDebug(const char *format, ...)
{
#ifndef NDEBUG
	char buf[4096];
	va_list ap;
	va_start(ap, format);
	_vsnprintf(buf, sizeof(buf), format, ap);
	OutputDebugStringA(buf);
	va_end(ap);
#else
	(void)format;
#endif
}

#ifdef NDEBUG
#define ErrorMessageBox(e, args...)	((void) 0)

#else
#define ErrorMessageBox(e, args...) _ErrorMessageBox(__FILE__, __LINE__, e, ## args)

#endif
#define FormatErrorMessage(n) \
({ \
	LPVOID lpMsgBuf; \
 \
	FormatMessage( \
		FORMAT_MESSAGE_ALLOCATE_BUFFER | \
		FORMAT_MESSAGE_FROM_SYSTEM | \
		FORMAT_MESSAGE_IGNORE_INSERTS, \
		NULL, \
		n, \
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
		(LPTSTR) &lpMsgBuf, \
		0, \
		NULL \
	); \
 \
	(LPSTR) lpMsgBuf; \
})
#define LastErrorMessage() FormatErrorMessage(GetLastError())
void _ErrorMessageBox (LPCTSTR lpszFile , DWORD dwLine , LPCTSTR lpszFormat , ... );
LPTSTR GetBaseName (LPTSTR lpFileName );
BOOL GetPlatformId (LPDWORD lpdwPlatformId );
BOOL ObtainSeDebugPrivilege (void);

#endif /* CFH_MISC_H */
