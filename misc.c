/* misc.c
 *
 *
 * Jose Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <windows.h>
#include <tchar.h>

#include "log.h"
#include "misc.h"

#ifdef HEADER

#include <stdio.h>

static inline void
	__attribute__ ((format (printf, 1, 2)))
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

#endif /* HEADER */

void _ErrorMessageBox(LPCTSTR lpszFile, DWORD dwLine, LPCTSTR lpszFormat, ...)
{
	TCHAR szErrorMsg[1024], szModule[MAX_PATH], szMsg[4096];
	va_list ap;
	
	if(!GetModuleFileName(NULL, szModule, MAX_PATH))
		lstrcpy(szModule, _T(""));

	va_start(ap, lpszFormat);
	wvsprintf(szErrorMsg, lpszFormat, ap);
	va_end(ap);

	wsprintf(
		szMsg,
		_T(
			"Error!\r\n"
			"\r\n"
			"Program: %s\r\n"
			"File: %s\r\n"
			"Line: %i\r\n"
			"\r\n"
			"%s\r\n"
			"\r\n"
			"(Press Retry to debug the application - JIT must be enabled)\r\n"
		),
		szModule,
		lpszFile,
		dwLine,
		szErrorMsg
	);

	// Display the string.
	switch(MessageBox(NULL, szMsg, _T("DrMingw"), MB_ICONERROR | MB_ABORTRETRYIGNORE))
	{
		case IDABORT:
			abort();
		
		case IDRETRY:
			DebugBreak();
			return;
		
		case IDIGNORE:
			return;
	}
}


