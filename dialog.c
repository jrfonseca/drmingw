/* dialog.c
 *
 *
 * José Fonseca (em96115@fe.up.pt)
 */

#include <windows.h>
#include <tchar.h>

#include "misc.h"
#include "resource.h"

static HINSTANCE hInstance = NULL;
static char szClassName[] = "MyWindowClass";

BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
   switch(Message)
   {
      case WM_INITDIALOG:

      return TRUE;
      case WM_COMMAND:
         switch(LOWORD(wParam))
         {
            case IDOK:
               EndDialog(hwnd, IDOK);
            return TRUE;
            case IDCANCEL:
               EndDialog(hwnd, IDCANCEL);
            return TRUE;
         }
      break;
   }
   return FALSE;
}

static HWND hEdit = NULL;

void UpdateText(LPCTSTR lpszBuffer)
{
	if(hEdit)
		if(
			!SendMessage(
				hEdit,	// handle to dialog box
				WM_SETTEXT,	// message to send
				(WPARAM) 0,	// not used; must be zero
				(LPARAM) lpszBuffer	// text to set
			)
		)
			ErrorMessageBox(_T("SendMessage: %s"), LastErrorMessage());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
		case WM_CREATE:
		{
			LOGFONT lf;  // structure for font information  
			
			hEdit = CreateWindow(
				"EDIT", 
				"",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				hwnd, 
				(HMENU)IDC_MESSAGE, 
				hInstance, 
				NULL
			);
			
			// Get a handle to the ANSI fixed-pitch font, and copy 
			// information about the font to a LOGFONT structure. 
			
			GetObject(GetStockObject(ANSI_FIXED_FONT), sizeof(LOGFONT), &lf); 
			
			// Set the font attributes, as appropriate.  
			
			lf.lfHeight = 10;
			lf.lfWidth = 0;
			
			// Create the font, and then return its handle.  
			
			SendDlgItemMessage(
				hwnd,
				IDC_MESSAGE,
				WM_SETFONT,
				(WPARAM) CreateFont(
					lf.lfHeight,
					lf.lfWidth, 
					lf.lfEscapement, 
					lf.lfOrientation, 
					lf.lfWeight, 
					lf.lfItalic, 
					lf.lfUnderline, 
					lf.lfStrikeOut, 
					lf.lfCharSet, 
					lf.lfOutPrecision, 
					lf.lfClipPrecision, 
					lf.lfQuality, 
					lf.lfPitchAndFamily, 
					lf.lfFaceName
				),
				MAKELPARAM(TRUE, 0)
			);
			break;
		}
		case WM_SIZE:
			if(wParam != SIZE_MINIMIZED)
				MoveWindow(GetDlgItem(hwnd, IDC_MESSAGE), 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
			break;
		case WM_SETFOCUS:
			SetFocus(GetDlgItem(hwnd, IDC_MESSAGE));
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case CM_FILE_SAVEAS:
				{
					OPENFILENAME ofn;
					char szFileName[MAX_PATH];
					
					ZeroMemory(&ofn, sizeof(ofn));
					szFileName[0] = 0;
					
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hwnd;
					ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0";
					ofn.lpstrFile = szFileName;
					ofn.nMaxFile = MAX_PATH;
					ofn.lpstrDefExt = "txt";
					
					ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
					OFN_OVERWRITEPROMPT;
					
					if(GetSaveFileName(&ofn))
					{
						HANDLE hFile;
						BOOL bSuccess = FALSE;
						
						if((hFile = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
						{
							DWORD dwTextLength;
							dwTextLength = GetWindowTextLength(hEdit);
							if(dwTextLength > 0) // No need to bother if there's no text.
							{
								LPSTR pszText;

								if((pszText = GlobalAlloc(GPTR, dwTextLength + 1)) != NULL)
								{
									if(GetWindowText(hEdit, pszText, dwTextLength + 1))
									{
										DWORD dwWritten;
										if(WriteFile(hFile, pszText, dwTextLength, &dwWritten, NULL))
											bSuccess = TRUE;
									}
									GlobalFree(pszText);
								}
							}
							CloseHandle(hFile);
						}
						
						if(!bSuccess)
				            MessageBox(hwnd, "Save file failed.", "Error", MB_OK | MB_ICONEXCLAMATION);
					}
					break;
				}
				case CM_FILE_EXIT:
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					break;

				case CM_HELP_ABOUT:
					return DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
      			}
			break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}


int Dialog(void)
{
	STARTUPINFO startinfo;
	WNDCLASSEX WndClass;
	HWND hwnd;
	MSG Msg;
	
	hInstance = GetModuleHandle (NULL);
	GetStartupInfoA (&startinfo);
	
	WndClass.cbSize        = sizeof(WNDCLASSEX);
	WndClass.style         = 0;
	WndClass.lpfnWndProc   = WndProc;
	WndClass.cbClsExtra    = 0;
	WndClass.cbWndExtra    = 0;
	WndClass.hInstance     = hInstance;
	WndClass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));
	WndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	WndClass.lpszMenuName  = MAKEINTRESOURCE(IDM_MAINMENU);
	WndClass.lpszClassName = szClassName;
	WndClass.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON)) /*LoadIcon(NULL, IDI_APPLICATION)*/;
	
	if(!RegisterClassEx(&WndClass))
	{
		ErrorMessageBox(_T("RegisterClassEx: %s"), LastErrorMessage());
		return 0;
	}

	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		szClassName,
		"Dr. Mingw",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, 
		NULL, 
		hInstance, 
		NULL
	);
	
	if(hwnd == NULL)
	{
		ErrorMessageBox(_T("CreateWindowEx: %s"), LastErrorMessage());
		return 0;
	}
	
	ShowWindow(hwnd, (startinfo.dwFlags & STARTF_USESHOWWINDOW) ? startinfo.wShowWindow : SW_SHOWDEFAULT);
	UpdateWindow(hwnd);
	
	while(GetMessage(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	
	return Msg.wParam;
}
