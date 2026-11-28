/*
 * Copyright 2026 Jose Fonseca
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


/*
 * End-to-end test of drmingw.exe acting as a JIT debugger.
 *
 * The same executable runs in two roles:
 *
 * - child (`test_drmingw.exe child`): crashes, and from its unhandled
 *   exception filter spawns `drmingw.exe -p <pid> -e <event>` the same way
 *   the system does when drmingw is registered as the AeDebug debugger.
 *
 * - driver (no arguments): spawns the child, waits for drmingw to terminate
 *   it, then locates drmingw's window, checks the stack trace it shows, and
 *   drives the Select All, Save As, About and Exit menu commands.
 */


#include "tap.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <string>

#include <windows.h>
#include <tlhelp32.h>
#include <richedit.h>

#include "resource.h" // drmingw's


#define TIMEOUT_MS 60000


/*
 * Child
 */

static int *volatile g_pNull = nullptr;


static __attribute__((noinline)) void
FaultingFunction(void)
{
    *g_pNull = 0;
}


static LONG WINAPI
jitFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
    (void)pExceptionInfo;

    // Mimic what the system does with the AeDebug "Debugger" command line:
    // create an inheritable event, launch the debugger with our process ID and
    // the event handle, and wait for the debugger to signal it once attached.
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE hEvent = CreateEventW(&sa, TRUE, FALSE, NULL);
    if (!hEvent) {
        fprintf(stderr, "child: CreateEvent failed (%lu)\n", GetLastError());
        ExitProcess(EXIT_FAILURE);
    }

    wchar_t szCommand[256];
    _snwprintf(szCommand, _countof(szCommand), L"drmingw.exe -p %lu -e %Iu",
               GetCurrentProcessId(), (UINT_PTR)hEvent);
    szCommand[_countof(szCommand) - 1] = L'\0';

    STARTUPINFOW si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(NULL, szCommand, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "child: CreateProcess(\"%ls\") failed (%lu)\n", szCommand,
                GetLastError());
        ExitProcess(EXIT_FAILURE);
    }
    CloseHandle(pi.hThread);

    HANDLE handles[2] = {hEvent, pi.hProcess};
    DWORD dwRet = WaitForMultipleObjects(2, handles, FALSE, TIMEOUT_MS);
    if (dwRet != WAIT_OBJECT_0) {
        fprintf(stderr, "child: drmingw failed to attach (%lu)\n", dwRet);
        ExitProcess(EXIT_FAILURE);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(hEvent);

    // The real JIT returns EXCEPTION_CONTINUE_SEARCH here, but from a
    // user-installed top-level filter that would make UnhandledExceptionFilter
    // proceed to launch the registered AeDebug debugger (or winedbg on Wine).
    // Instead re-execute the faulting instruction: now that a debugger is
    // attached, the exception goes to drmingw as first chance, and, since
    // UnhandledExceptionFilter returns EXCEPTION_CONTINUE_SEARCH when it
    // detects a debugger, as second chance too, whereupon drmingw terminates
    // this process.
    return EXCEPTION_CONTINUE_EXECUTION;
}


static int
child(void)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(jitFilter);

    FaultingFunction();

    fprintf(stderr, "child: unexpectedly survived the fault\n");
    return EXIT_FAILURE;
}


/*
 * Driver
 */

static DWORD
findChildProcess(DWORD dwParentProcessId, const wchar_t *szExeFile)
{
    DWORD dwProcessId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof pe32;
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ParentProcessID == dwParentProcessId &&
                    _wcsicmp(pe32.szExeFile, szExeFile) == 0) {
                    dwProcessId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return dwProcessId;
}


struct FindWindowData {
    DWORD dwProcessId;
    const wchar_t *szClassName;
    HWND hWnd;
};


static BOOL CALLBACK
findWindowProc(HWND hWnd, LPARAM lParam)
{
    FindWindowData *pData = reinterpret_cast<FindWindowData *>(lParam);
    DWORD dwProcessId = 0;
    GetWindowThreadProcessId(hWnd, &dwProcessId);
    if (dwProcessId == pData->dwProcessId) {
        wchar_t szClassName[256];
        if (GetClassNameW(hWnd, szClassName, _countof(szClassName)) &&
            wcscmp(szClassName, pData->szClassName) == 0) {
            pData->hWnd = hWnd;
            return FALSE;
        }
    }
    return TRUE;
}


// Find a top-level window of the given process and class, waiting for it to
// appear.
static HWND
waitForWindow(DWORD dwProcessId, const wchar_t *szClassName)
{
    DWORD dwStart = GetTickCount();
    do {
        FindWindowData data = {dwProcessId, szClassName, NULL};
        EnumWindows(findWindowProc, reinterpret_cast<LPARAM>(&data));
        if (data.hWnd) {
            return data.hWnd;
        }
        Sleep(100);
    } while (GetTickCount() - dwStart < TIMEOUT_MS);
    return NULL;
}


static std::wstring
getWindowText(HWND hWnd)
{
    DWORD_PTR dwLength = 0;
    if (!SendMessageTimeoutW(hWnd, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG, TIMEOUT_MS,
                             &dwLength)) {
        return std::wstring();
    }
    std::wstring text(dwLength + 1, L'\0');
    DWORD_PTR dwCopied = 0;
    if (!SendMessageTimeoutW(hWnd, WM_GETTEXT, (WPARAM)text.size(), (LPARAM)&text[0],
                             SMTO_ABORTIFHUNG, TIMEOUT_MS, &dwCopied)) {
        return std::wstring();
    }
    text.resize(dwCopied);
    return text;
}


static size_t
countOccurrences(const std::wstring &haystack, const wchar_t *needle)
{
    size_t count = 0;
    size_t len = wcslen(needle);
    for (size_t pos = haystack.find(needle); pos != std::wstring::npos;
         pos = haystack.find(needle, pos + len)) {
        ++count;
    }
    return count;
}


// The dump is appended asynchronously via posted messages, so poll until it
// contains the expected number of occurrences of the needle and stops growing.
static std::wstring
waitForText(HWND hEdit, const wchar_t *szNeedle, size_t count)
{
    std::wstring text;
    DWORD dwStart = GetTickCount();
    do {
        std::wstring prevText = text;
        text = getWindowText(hEdit);
        if (countOccurrences(text, szNeedle) >= count && text == prevText) {
            break;
        }
        Sleep(250);
    } while (GetTickCount() - dwStart < TIMEOUT_MS);
    return text;
}


// Get a rich edit control's selected text.  EM_GETSELTEXT is not marshalled
// across processes, so the buffer must live in the control's process.
static std::wstring
getSelText(HANDLE hProcess, HWND hEdit)
{
    std::wstring text;

    DWORD_PTR dwLength = 0;
    if (!SendMessageTimeoutW(hEdit, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG, TIMEOUT_MS,
                             &dwLength)) {
        return text;
    }
    SIZE_T cbBuffer = (dwLength + 1) * sizeof(wchar_t);

    void *pRemote = VirtualAllocEx(hProcess, NULL, cbBuffer, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemote) {
        return text;
    }

    DWORD_PTR dwCopied = 0;
    if (SendMessageTimeoutW(hEdit, EM_GETSELTEXT, 0, (LPARAM)pRemote, SMTO_ABORTIFHUNG,
                            TIMEOUT_MS, &dwCopied) &&
        dwCopied <= dwLength) {
        text.resize(dwCopied);
        if (!ReadProcessMemory(hProcess, pRemote, &text[0], dwCopied * sizeof(wchar_t), NULL)) {
            text.clear();
        }
    }

    VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);

    return text;
}


static bool
waitForWindowGone(HWND hWnd)
{
    DWORD dwStart = GetTickCount();
    while (IsWindow(hWnd) && GetTickCount() - dwStart < TIMEOUT_MS) {
        Sleep(100);
    }
    return !IsWindow(hWnd);
}


struct FindChildData {
    int nId;
    const wchar_t *szClassName;
    HWND hWnd;
};


static BOOL CALLBACK
findChildProc(HWND hWnd, LPARAM lParam)
{
    FindChildData *pData = reinterpret_cast<FindChildData *>(lParam);
    if (pData->nId && GetDlgCtrlID(hWnd) != pData->nId) {
        return TRUE;
    }
    if (pData->szClassName) {
        wchar_t szClassName[256];
        if (!GetClassNameW(hWnd, szClassName, _countof(szClassName)) ||
            _wcsicmp(szClassName, pData->szClassName) != 0) {
            return TRUE;
        }
    }
    pData->hWnd = hWnd;
    return FALSE;
}


// Find a descendant window by control ID and/or class name.
static HWND
findDescendant(HWND hParent, int nId, const wchar_t *szClassName)
{
    FindChildData data = {nId, szClassName, NULL};
    EnumChildWindows(hParent, findChildProc, reinterpret_cast<LPARAM>(&data));
    return data.hWnd;
}


static std::wstring
getClipboardText(void)
{
    std::wstring text;
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t *pData = static_cast<const wchar_t *>(GlobalLock(hData));
            if (pData) {
                text = pData;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    return text;
}


// Drive drmingw's File > Save As common dialog to save to the given path.
static bool
saveAs(DWORD dwProcessId, HWND hWnd, const wchar_t *szFileName)
{
    // The dialog is modal, so the command must be posted rather than sent.
    PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(CM_FILE_SAVEAS, 0), 0);
    HWND hDlg = waitForWindow(dwProcessId, L"#32770");
    test_line(hDlg != NULL, "save as dialog");
    if (!hDlg) {
        return false;
    }

    // The file name box is the cmb13 combo box (an edit control in older
    // templates), possibly nested inside other windows.
    const int cmb13 = 0x47c;
    const int edt1 = 0x480;
    HWND hFileName = findDescendant(hDlg, cmb13, NULL);
    if (hFileName) {
        HWND hComboEdit = findDescendant(hFileName, 0, L"Edit");
        if (hComboEdit) {
            hFileName = hComboEdit;
        }
    } else {
        hFileName = findDescendant(hDlg, edt1, NULL);
    }
    test_line(hFileName != NULL, "save as file name box");
    if (!hFileName) {
        PostMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
        waitForWindowGone(hDlg);
        return false;
    }

    DWORD_PTR dwResult;
    SendMessageTimeoutW(hFileName, WM_SETTEXT, 0, (LPARAM)szFileName, SMTO_ABORTIFHUNG,
                        TIMEOUT_MS, &dwResult);
    PostMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
                 (LPARAM)GetDlgItem(hDlg, IDOK));

    bool ok = waitForWindowGone(hDlg);
    test_line(ok, "save as dialog dismissed");
    if (!ok) {
        return false;
    }

    // The file is written after the dialog returns, so wait for drmingw to
    // be back in its main message loop.
    SendMessageTimeoutW(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, TIMEOUT_MS, &dwResult);

    return true;
}


// Read a file saved by drmingw, which writes the raw UTF-16 text.
static std::wstring
readUtf16File(const wchar_t *szFileName)
{
    std::wstring text;
    HANDLE hFile = CreateFileW(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwSize = GetFileSize(hFile, NULL);
        if (dwSize != INVALID_FILE_SIZE) {
            text.resize(dwSize / sizeof(wchar_t));
            DWORD dwRead = 0;
            if (!ReadFile(hFile, &text[0], DWORD(text.size() * sizeof(wchar_t)), &dwRead, NULL)) {
                dwRead = 0;
            }
            text.resize(dwRead / sizeof(wchar_t));
        }
        CloseHandle(hFile);
    }
    return text;
}


// Rich edit controls use CR as paragraph separator internally, but CRLF in
// WM_GETTEXT, so normalize line endings to LF before comparing.
static std::wstring
normalizeNewlines(const std::wstring &text)
{
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') {
                ++i;
            }
            result += L'\n';
        } else {
            result += text[i];
        }
    }
    return result;
}


static void
testWindow(DWORD dwDrMingwProcessId, HANDLE hDrMingwProcess)
{
    bool ok;

    HWND hWnd = waitForWindow(dwDrMingwProcessId, L"DrMingw");
    test_line(hWnd != NULL, "drmingw window");
    if (!hWnd) {
        return;
    }

    wchar_t szTitle[256] = L"";
    GetWindowTextW(hWnd, szTitle, _countof(szTitle));
    test_line(wcscmp(szTitle, L"Dr. Mingw") == 0, "window title \"%ls\"", szTitle);

    HWND hEdit = GetDlgItem(hWnd, IDC_MESSAGE);
    test_line(hEdit != NULL, "GetDlgItem(IDC_MESSAGE)");
    if (!hEdit) {
        return;
    }

    // drmingw dumps both the first and the second chance exceptions
    static const wchar_t szException[] = L"test_drmingw.exe caused an Access Violation";
    std::wstring text = waitForText(hEdit, L"Modules:", 2);
    fprintf(stdout, "%ls\n", text.c_str());
    fflush(stdout);

    ok = countOccurrences(text, szException) == 2;
    test_line(ok, "exception description");

    ok = countOccurrences(text, L"test_drmingw.exe!FaultingFunction") == 2;
    test_line(ok, "faulting function in stack trace");

    ok = text.find(L"test_drmingw.cpp") != std::wstring::npos;
    test_line(ok, "source file in stack trace");

    // Edit > Select All
    SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(CM_EDIT_SELECTALL, 0), 0);
    std::wstring selection = normalizeNewlines(getSelText(hDrMingwProcess, hEdit));
    ok = !selection.empty() && selection == normalizeNewlines(text);
    test_line(ok, "selection after Select All (%u characters)", (unsigned)selection.size());

    // Edit > Copy.  This clobbers the clipboard, so only do it on CI.
    const char *szGitHubActions = getenv("GITHUB_ACTIONS");
    if (szGitHubActions && strcmp(szGitHubActions, "true") == 0) {
        if (OpenClipboard(NULL)) {
            EmptyClipboard();
            CloseClipboard();
        }
        SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(CM_EDIT_COPY, 0), 0);
        std::wstring clipboard = normalizeNewlines(getClipboardText());
        ok = !clipboard.empty() && clipboard == selection;
        test_line(ok, "clipboard after Copy matches selection (%u characters)",
                  (unsigned)clipboard.size());
    } else {
        test_diagnostic("skipping Copy, as GITHUB_ACTIONS is not \"true\"");
    }

    // File > Save As
    wchar_t szTempPath[MAX_PATH];
    GetTempPathW(_countof(szTempPath), szTempPath);
    wchar_t szFileName[MAX_PATH];
    _snwprintf(szFileName, _countof(szFileName), L"%lstest_drmingw_%lu.txt", szTempPath,
               GetCurrentProcessId());
    szFileName[_countof(szFileName) - 1] = L'\0';
    DeleteFileW(szFileName);
    if (saveAs(dwDrMingwProcessId, hWnd, szFileName)) {
        std::wstring saved = normalizeNewlines(readUtf16File(szFileName));
        ok = !saved.empty() && saved == selection;
        test_line(ok, "saved file \"%ls\" matches selection (%u characters)", szFileName,
                  (unsigned)saved.size());
    }
    DeleteFileW(szFileName);

    // Help > About, which is modal so must be posted rather than sent
    PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(CM_HELP_ABOUT, 0), 0);
    HWND hAbout = waitForWindow(dwDrMingwProcessId, L"#32770");
    test_line(hAbout != NULL, "about dialog");
    if (hAbout) {
        PostMessageW(hAbout, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
        test_line(waitForWindowGone(hAbout), "about dialog dismissed");
    }

    // File > Exit
    PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(CM_FILE_EXIT, 0), 0);
    ok = WaitForSingleObject(hDrMingwProcess, TIMEOUT_MS) == WAIT_OBJECT_0;
    test_line(ok, "drmingw exited");
    if (ok) {
        DWORD dwExitCode = STILL_ACTIVE;
        GetExitCodeProcess(hDrMingwProcess, &dwExitCode);
        test_line(dwExitCode == 0, "drmingw exit code 0x%lx", dwExitCode);
    }
}


static int
driver(void)
{
    bool ok;

    wchar_t szModule[MAX_PATH];
    GetModuleFileNameW(NULL, szModule, _countof(szModule));

    wchar_t szCommand[MAX_PATH + 32];
    _snwprintf(szCommand, _countof(szCommand), L"\"%ls\" child", szModule);
    szCommand[_countof(szCommand) - 1] = L'\0';

    STARTUPINFOW si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    ok = CreateProcessW(NULL, szCommand, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) != FALSE;
    test_line(ok, "CreateProcessW(\"%ls\")", szCommand);
    if (!ok) {
        test_diagnostic_last_error();
        test_exit();
    }
    CloseHandle(pi.hThread);

    ok = WaitForSingleObject(pi.hProcess, TIMEOUT_MS) == WAIT_OBJECT_0;
    test_line(ok, "child terminated");
    if (!ok) {
        TerminateProcess(pi.hProcess, EXIT_FAILURE);
    }

    DWORD dwExitCode = STILL_ACTIVE;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    test_line(dwExitCode == EXCEPTION_ACCESS_VIOLATION, "child exit code 0x%lx", dwExitCode);
    CloseHandle(pi.hProcess);

    // drmingw outlives the child, waiting for the user to close its window
    DWORD dwDrMingwProcessId = findChildProcess(pi.dwProcessId, L"drmingw.exe");
    test_line(dwDrMingwProcessId != 0, "drmingw process");
    if (dwDrMingwProcessId) {
        HANDLE hDrMingwProcess =
            OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE |
                            PROCESS_VM_OPERATION | PROCESS_VM_READ,
                        FALSE, dwDrMingwProcessId);
        test_line(hDrMingwProcess != NULL, "OpenProcess(drmingw)");
        if (hDrMingwProcess) {
            testWindow(dwDrMingwProcessId, hDrMingwProcess);
            if (WaitForSingleObject(hDrMingwProcess, 0) != WAIT_OBJECT_0) {
                TerminateProcess(hDrMingwProcess, EXIT_FAILURE);
            }
            CloseHandle(hDrMingwProcess);
        }
    }

    test_exit();
}


int
main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "child") == 0) {
        return child();
    }

    return driver();
}
