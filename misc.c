/* misc.c
 *
 *
 * José Fonseca (em96115@fe.up.pt)
 */

#include <windows.h>
#include <tchar.h>

#include "log.h"
#include "misc.h"

int verbose_flag = 0;	/* Verbose output (default=no).  */

#ifdef HEADER

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


LPTSTR GetBaseName(LPTSTR lpFileName)
{
	LPTSTR lpChar = lpFileName + lstrlen(lpFileName);
	
	while(lpChar > lpFileName && *(lpChar - 1) != '\\' && *(lpChar - 1) != '/' && *(lpChar - 1) != ':')
		--lpChar;
	
	return lpChar;
}


BOOL GetPlatformId(LPDWORD lpdwPlatformId )
{
	OSVERSIONINFO VersionInfo;
	BOOL bResult;
	
	VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	
	if((bResult = GetVersionEx(&VersionInfo)))
		*lpdwPlatformId = VersionInfo.dwPlatformId;
	
	return bResult;
}


/*
BOOL SetPrivilege(HANDLE hToken, LPCTSTR Privilege, BOOL bEnablePrivilege)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;
	TOKEN_PRIVILEGES tpPrevious;
	DWORD cbPrevious=sizeof(TOKEN_PRIVILEGES);

	if(!LookupPrivilegeValue( NULL, Privilege, &luid ))
		return FALSE;

	// first pass.  get current privilege setting
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		&tpPrevious,
		&cbPrevious
	);

	if (GetLastError() != ERROR_SUCCESS)
		return FALSE;

	// second pass.  set privilege based on previous setting
	tpPrevious.PrivilegeCount = 1;
	tpPrevious.Privileges[0].Luid = luid;

	if(bEnablePrivilege)
	{
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	}
	else 
	{
		tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED &
			tpPrevious.Privileges[0].Attributes);
	}

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tpPrevious,
		cbPrevious,
		NULL,
		NULL
	);

	if (GetLastError() != ERROR_SUCCESS)
		return FALSE;

	return TRUE;
}

BOOL ObtainSeDebugPrivilege(void)
{
	DWORD dwProcessId;
	
	GetPlatformId(&dwProcessId);
	if(dwProcessId == VER_PLATFORM_WIN32_NT)
	{
		HANDLE hToken;
		BOOL bResult;
	
		if(!OpenProcessToken(
				GetCurrentProcess(),
				TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
				&hToken
			))
			return FALSE;

		// enable SeDebugPrivilege
		bResult = SetPrivilege(hToken, SE_DEBUG_NAME, TRUE);

		// close handles
		CloseHandle(hToken);
		
		return bResult;
	}
	else
		return TRUE;
}
*/

BOOL ObtainSeDebugPrivilege(void)
{
	DWORD dwProcessId;
	
	GetPlatformId(&dwProcessId);
	if(dwProcessId == VER_PLATFORM_WIN32_NT)
	{
		HANDLE hToken;
		PTOKEN_PRIVILEGES NewPrivileges;
		BYTE OldPriv[1024];
		PBYTE pbOldPriv;
		ULONG cbNeeded;
		BOOLEAN fRc;
		LUID LuidPrivilege;
	
		// Make sure we have access to adjust and to get the old token privileges
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		{
			if(verbose_flag)
				ErrorMessageBox(_T("OpenProcessToken: %s"), LastErrorMessage());
				
			return FALSE;
		}
	
		cbNeeded = 0;
	
		// Initialize the privilege adjustment structure
		LookupPrivilegeValue( NULL, SE_DEBUG_NAME, &LuidPrivilege );
	
		NewPrivileges = (PTOKEN_PRIVILEGES) LocalAlloc(
			LMEM_ZEROINIT,
			sizeof(TOKEN_PRIVILEGES) + (1 - ANYSIZE_ARRAY)*sizeof(LUID_AND_ATTRIBUTES)
		);
		if(NewPrivileges == NULL)
		{
			if(verbose_flag)
				ErrorMessageBox(_T("LocalAlloc: %s"), LastErrorMessage());

			return FALSE;
		}
	
		NewPrivileges->PrivilegeCount = 1;
		NewPrivileges->Privileges[0].Luid = LuidPrivilege;
		NewPrivileges->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	
		// Enable the privilege
		pbOldPriv = OldPriv;
		fRc = AdjustTokenPrivileges(
			hToken,
			FALSE,
			NewPrivileges,
			1024,
			(PTOKEN_PRIVILEGES)pbOldPriv,
			&cbNeeded
		);
	
		if (!fRc) {
	
			// If the stack was too small to hold the privileges
			// then allocate off the heap
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
	
				pbOldPriv = LocalAlloc(LMEM_FIXED, cbNeeded);
				if (pbOldPriv == NULL)
				{
					if(verbose_flag)
						ErrorMessageBox(_T("LocalAlloc: %s"), LastErrorMessage());
					return FALSE;
				}
	
				fRc = AdjustTokenPrivileges(
					hToken,
					FALSE,
					NewPrivileges,
					cbNeeded,
					(PTOKEN_PRIVILEGES)pbOldPriv,
					&cbNeeded
				);
			}
		}
		return fRc;
	}
	else
		return TRUE;
}
