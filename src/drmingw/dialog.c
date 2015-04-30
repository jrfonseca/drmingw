/*
 * Copyright 2002-2013 Jose Fonseca
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <windows.h>
#include <tchar.h>

#include "errmsg.h"
#include "resource.h"
#include "dialog.h"

static HINSTANCE g_hInstance = NULL;

static HWND g_hWnd = NULL;


#define WM_USER_APPEND_TEXT (WM_USER + 1)

static INT_PTR CALLBACK
AboutDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch(Message) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
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


void
appendText(LPCTSTR szText)
{
    char *szBuf = strdup(szText);
    PostMessage(g_hWnd, WM_USER_APPEND_TEXT, (WPARAM) 0, (LPARAM) szBuf);
}


static LRESULT CALLBACK
WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch(Message)
    {
        case WM_CREATE:
        {
            CreateWindow(
                "EDIT",
                "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd,
                (HMENU)IDC_MESSAGE,
                g_hInstance,
                NULL
            );

            // We used to use GetStockObject(ANSI_FIXED_FONT), but it's known
            // to lead to unreliable results, particularly on Russion locales
            // or high-DPI displays, so now we match Notepad's default font.
            LOGFONT lf = {
                10,                      // lfHeight
                0,                       // lfWidth
                0,                       // lfEscapement
                0,                       // lfOrientation
                FW_NORMAL,               // lfWeight
                FALSE,                   // lfItalic
                FALSE,                   // lfUnderline
                FALSE,                   // lfStrikeOut
                ANSI_CHARSET,            // lfCharSet
                0,                       // lfOutPrecision
                0,                       // lfClipPrecision
                DEFAULT_QUALITY,         // lfQuality
                FIXED_PITCH | FF_MODERN, // lfPitchAndFamily
                _T("Lucida Console")     // lfFaceName
            };

            // Apply the DPI scale factor
            // https://msdn.microsoft.com/en-us/library/windows/desktop/dn469266.aspx
            if (lf.lfHeight > 0) {
                HDC hdc = GetDC(NULL);
                int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
                ReleaseDC(NULL, hdc);

                lf.lfHeight = -MulDiv(lf.lfHeight, dpiY, 72);
            }

            HFONT hFont;
            hFont = CreateFontIndirect(&lf);

            SendDlgItemMessage(hwnd, IDC_MESSAGE, WM_SETFONT, (WPARAM) hFont, MAKELPARAM(TRUE, 0));
            break;
        }
        case WM_USER_APPEND_TEXT: {
            // http://support.microsoft.com/kb/109550
            HWND hEdit = GetDlgItem(hwnd, IDC_MESSAGE);
            int ndx = GetWindowTextLength(hEdit);
            SetFocus(hEdit);
            SendMessage(hEdit, EM_SETSEL, (WPARAM) ndx, (LPARAM) ndx);
            SendMessage(hEdit, EM_REPLACESEL, (WPARAM) 0, lParam);
            free((void *)lParam);
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
                            HWND hEdit = GetDlgItem(hwnd, IDC_MESSAGE);
                            DWORD dwTextLength = GetWindowTextLength(hEdit);
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
                    return DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
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


void
createDialog(void)
{
    STARTUPINFO startinfo;
    WNDCLASSEX WndClass;

    g_hInstance = GetModuleHandle(NULL);
    GetStartupInfoA(&startinfo);

    WndClass.cbSize        = sizeof(WNDCLASSEX);
    WndClass.style         = 0;
    WndClass.lpfnWndProc   = WndProc;
    WndClass.cbClsExtra    = 0;
    WndClass.cbWndExtra    = 0;
    WndClass.hInstance     = g_hInstance;
    WndClass.hIcon         = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_MAINICON));
    WndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    WndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    WndClass.lpszMenuName  = MAKEINTRESOURCE(IDM_MAINMENU);
    WndClass.lpszClassName = "DrMingw";
    WndClass.hIconSm       = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_MAINICON)) /*LoadIcon(NULL, IDI_APPLICATION)*/;

    if (!RegisterClassEx(&WndClass)) {
        ErrorMessageBox(_T("RegisterClassEx: %s"), LastErrorMessage());
        exit(EXIT_FAILURE);
    }

    g_hWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        WndClass.lpszClassName,
        "Dr. Mingw",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        g_hInstance,
        NULL
    );

    if (g_hWnd == NULL) {
        ErrorMessageBox(_T("CreateWindowEx: %s"), LastErrorMessage());
        exit(EXIT_FAILURE);
    }

    ShowWindow(g_hWnd, (startinfo.dwFlags & STARTF_USESHOWWINDOW) ? startinfo.wShowWindow : SW_SHOWDEFAULT);
    UpdateWindow(g_hWnd);
}


int
mainLoop(void)
{
    MSG Msg;
    BOOL bRet;

    while ((bRet = GetMessage(&Msg, NULL, 0, 0)) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    if (bRet < 0) {
        return EXIT_FAILURE;
    }

    return Msg.wParam;
}
