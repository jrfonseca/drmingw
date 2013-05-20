/* symbols.c
 *
 *
 * Jose Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <psapi.h>

#include "misc.h"
#include "pehelp.h"
#include "mgwhelp.h"
#include "symbols.h"


#ifdef HEADER
#include <dbghelp.h>
#endif /* HEADER */

BOOL bSymInitialized = FALSE;

BOOL GetSymFromAddr(HANDLE hProcess, DWORD64 dwAddress, LPTSTR lpSymName, DWORD nSize)
{
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)malloc(sizeof(SYMBOL_INFO) + nSize * sizeof(TCHAR));

	DWORD64 dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol
	BOOL bRet;

	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = nSize;

	assert(bSymInitialized);
	
	bRet = MgwSymFromAddr(hProcess, dwAddress, &dwDisplacement, pSymbol);

	if (bRet) {
		lstrcpyn(lpSymName, pSymbol->Name, nSize);
	}

	free(pSymbol);

	return bRet;
}

BOOL GetLineFromAddr(HANDLE hProcess, DWORD64 dwAddress,  LPTSTR lpFileName, DWORD nSize, LPDWORD lpLineNumber)
{
	IMAGEHLP_LINE64 Line;
	DWORD dwDisplacement = 0;  // Displacement of the input address, relative to the start of the symbol

	// Do the source and line lookup.
	memset(&Line, 0, sizeof Line);
	Line.SizeOfStruct = sizeof Line;
	
	assert(bSymInitialized);

#if 0
	{
		// The problem is that the symbol engine only finds those source
		//  line addresses (after the first lookup) that fall exactly on
		//  a zero displacement.  I will walk backwards 100 bytes to
		//  find the line and return the proper displacement.
		DWORD64 dwTempDisp = 0 ;
		while (dwTempDisp < 100 && !MgwSymGetLineFromAddr64(hProcess, dwAddress - dwTempDisp, &dwDisplacement, &Line))
			++dwTempDisp;
		
		if(dwTempDisp >= 100)
			return FALSE;
			
		// It was found and the source line information is correct so
		//  change the displacement if it was looked up multiple times.
		if (dwTempDisp < 100 && dwTempDisp != 0 )
			dwDisplacement = dwTempDisp;
	}
#else
	if(!MgwSymGetLineFromAddr64(hProcess, dwAddress, &dwDisplacement, &Line))
		return FALSE;
#endif

	assert(lpFileName && lpLineNumber);
	
	lstrcpyn(lpFileName, Line.FileName, nSize);
	*lpLineNumber = Line.LineNumber;
	
	return TRUE;
}

