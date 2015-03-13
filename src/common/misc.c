/* misc.c
 *
 *
 * Jose Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <windows.h>
#include <tchar.h>

#include "misc.h"


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
    switch (MessageBox(NULL, szMsg, _T("DrMingw"), MB_ICONERROR | MB_ABORTRETRYIGNORE))
    {
        case IDABORT:
            _exit(3);
	    return;

        case IDRETRY:
            DebugBreak();
            return;

        case IDIGNORE:
            return;
    }
}


