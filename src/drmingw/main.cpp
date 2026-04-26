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

#include <assert.h>
#include <stdlib.h>

#include <string>

#include <windows.h>
#include <tlhelp32.h>

#include <process.h>

#include "getoptW.h"
#include "debugger.h"
#include "symbols.h"
#include "dialog.h"
#include "errmsg.h"
#include "log.h"
#include "outdbg.h"
#include "version.h"


static int process_id_given = 0; /* Whether process-id was given.  */
static int install_given = 0;    /* Whether install was given.  */
static int uninstall_given = 0;  /* Whether uninstall was given.  */


static void
help(void)
{
    MessageBoxW(NULL,
                L"" VER_PRODUCTNAME_STR L" version " VER_PRODUCTVERSION_STR L"\r\n"
                L"" VER_LEGALCOPYRIGHT_STR L"\r\n"
                L"\r\n"
                L"Usage: drmingw [OPTIONS]\r\n"
                L"\r\n"
                L"Options:\r\n"
                L"  -h, --help\tPrint help and exit\r\n"
                L"  -V, --version\tPrint version and exit\r\n"
                L"  -i, --install\tInstall as the default JIT debugger\r\n"
                L"  -u, --uninstall\tUninstall\r\n"
                L"  -pLONG, --process-id=LONG\r\n"
                L"\t\tAttach to the process with the given identifier\r\n"
                L"  -eLONG, --event=LONG\r\n"
                L"\t\tSignal an event after process is attached\r\n"
                L"  -b, --breakpoint\tTreat debug breakpoints as exceptions\r\n"
                L"  -v, --verbose\tVerbose output\r\n"
                L"  -d, --debug\tDebug output\r\n",
                L"" PACKAGE, MB_OK | MB_ICONINFORMATION);
}


static LSTATUS
regSetStr(HKEY hKey, LPCWSTR lpValueName, LPCWSTR szStr)
{
    return RegSetValueExW(hKey, lpValueName, 0, REG_SZ, reinterpret_cast<LPCBYTE>(szStr),
                          (wcslen(szStr) + 1) * sizeof(wchar_t));
}


static DWORD
install(REGSAM samDesired)
{
    wchar_t szFile[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szFile, MAX_PATH)) {
        return GetLastError();
    }

    std::wstring debuggerCommand;
    debuggerCommand += L'"';
    debuggerCommand += szFile;
    debuggerCommand += L"\" -p %ld -e %ld";
    if (debugOptions.verbose_flag)
        debuggerCommand += L" -v";
    if (debugOptions.breakpoint_flag)
        debuggerCommand += L" -b";
    if (debugOptions.debug_flag)
        debuggerCommand += L" -d";

    HKEY hKey;
    long lRet;
    DWORD dwDisposition;

    lRet =
        RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug", // The AeDebug
                                                                                     // registry key.
                        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE | samDesired, NULL, &hKey,
                        &dwDisposition);
    if (lRet != ERROR_SUCCESS) {
        return lRet;
    }

    // Write the Debugger value.
    lRet = regSetStr(hKey, L"Debugger", debuggerCommand.c_str());
    if (lRet == ERROR_SUCCESS) {
        // Write the Auto value.
        lRet = regSetStr(hKey, L"Auto", L"1");
    }

    // Close the key no matter what happened.
    RegCloseKey(hKey);

    return lRet;
}


static DWORD
uninstall(REGSAM samDesired)
{
    HKEY hKey;
    long lRet;

    lRet =
        RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug", // The AeDebug
                                                                                   // registry key.
                      0, KEY_ALL_ACCESS | samDesired, &hKey);
    if (lRet != ERROR_SUCCESS) {
        return lRet;
    }

    // Write the Debugger value.
    lRet = regSetStr(hKey, L"Debugger", L"");

    // Leave Auto value as "1".  It is the default
    // (https://docs.microsoft.com/en-us/windows/desktop/Debug/configuring-automatic-debugging)
    // and setting it to "0" doesn't seem to work as documented on Windows 10
    // (https://github.com/jrfonseca/drmingw/issues/38)

    // Close the key no matter what happened.
    RegCloseKey(hKey);

    return lRet;
}


static DWORD
getProcessIdByName(const wchar_t *szProcessName)
{
    DWORD dwProcessId = 0;

    HANDLE hProcessSnap;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof pe32;
        if (Process32FirstW(hProcessSnap, &pe32)) {
            do {
                if (_wcsicmp(szProcessName, pe32.szExeFile) == 0) {
                    dwProcessId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);
    }

    return dwProcessId;
}


static void
debugThread(void *arg)
{
    DWORD dwProcessId = (DWORD)(UINT_PTR)arg;

    // attach debuggee
    if (!DebugActiveProcess(dwProcessId)) {
        ErrorMessageBox("DebugActiveProcess: %s", LastErrorMessage());
        return;
    }

    setDumpCallback(appendText);

    SetSymOptions(debugOptions.debug_flag);

    DebugMainLoop();
}


int
wmain(int argc, wchar_t **argv)
{
    DWORD dwProcessId = 0;
    int c; /* Character of the parsed option.  */

    debugOptions.first_chance = 1;

    while (1) {
        int option_index = 0;
        static const struct option long_options[] = {{L"help", 0, NULL, 'h'},
                                                     {L"version", 0, NULL, 'V'},
                                                     {L"install", 0, NULL, 'i'},
                                                     {L"uninstall", 0, NULL, 'u'},
                                                     {L"process-id", 1, NULL, 'p'},
                                                     {L"event", 1, NULL, 'e'},
                                                     {L"tid", 1, NULL, 't'},
                                                     {L"breakpoint", 0, NULL, 'b'},
                                                     {L"verbose", 0, NULL, 'v'},
                                                     {L"debug", 0, NULL, 'd'},
                                                     {NULL, 0, NULL, 0}};

        c = getoptW_long_only(argc, argv, L"?hViup:e:t:vbd", long_options, &option_index);

        if (c == -1)
            break; /* Exit from `while (1)' loop.  */

        switch (c) {
        case '?':
            if (optopt != '?') {
                /* Invalid option.  */
                wchar_t szErrMsg[512];
                swprintf(szErrMsg, 512, L"Invalid option '%c'", optopt);
                MessageBoxW(NULL, szErrMsg, L"" PACKAGE, MB_OK | MB_ICONSTOP);
                return 1;
            }
            /* fall-through */
        case 'h': /* Print help and exit.  */
            help();
            return 0;

        case 'V': /* Print version and exit.  */
            MessageBoxW(NULL, L"" PACKAGE L" " VERSION, L"" PACKAGE, MB_OK | MB_ICONINFORMATION);
            return 0;

        case 'i': /* Install as the default JIT debugger.  */
            if (uninstall_given) {
                MessageBoxW(NULL,
                            L"conficting options `--uninstall' (`-u') and `--install' (`-i')",
                            L"" PACKAGE, MB_OK | MB_ICONSTOP);
                return 0;
            }
            install_given = 1;
            break;

        case 'u': /* Uninstall.  */
            if (install_given) {
                MessageBoxW(NULL,
                            L"conficting options `--install' (`-i') and `--uninstall' (`-u')",
                            L"" PACKAGE, MB_OK | MB_ICONSTOP);
                return 0;
            }
            uninstall_given = 1;
            break;

        case 'p': /* Attach to the process with the given identifier.  */
            if (process_id_given) {
                MessageBoxW(NULL, L"`--process-id' (`-p') option redeclared", L"" PACKAGE,
                            MB_OK | MB_ICONSTOP);
                return 1;
            }
            process_id_given = 1;
            if (optarg[0] >= L'0' && optarg[0] <= L'9') {
                dwProcessId = wcstoul(optarg, NULL, 0);
            } else {
                debugOptions.breakpoint_flag = true;
                dwProcessId = getProcessIdByName(optarg);
            }
            if (!dwProcessId) {
                MessageBoxW(NULL, L"Invalid process", L"" PACKAGE, MB_OK | MB_ICONSTOP);
                return 1;
            }
            break;

        case 'e': /* Signal an event after process is attached.  */
            if (debugOptions.hEvent) {
                MessageBoxW(NULL, L"`--event' (`-e') option redeclared", L"" PACKAGE,
                            MB_OK | MB_ICONSTOP);
                return 1;
            }
            debugOptions.hEvent = (HANDLE)(INT_PTR)wcstol(optarg, NULL, 0);
            break;

        case 't': {
            /* Thread id.  Used when debugging WinRT apps. */
            debugOptions.dwThreadId = wcstoul(optarg, NULL, 0);
            break;
        }

        case 'b': /* Treat debug breakpoints as exceptions */
            debugOptions.breakpoint_flag = true;
            break;

        case 'v': /* Verbose output.  */
            debugOptions.verbose_flag = true;
            break;

        case 'd': /* Debug output.  */
            debugOptions.debug_flag = true;
            break;

        default: /* bug: option not considered.  */
        {
            wchar_t szErrMsg[512];
            swprintf(szErrMsg, 512, L"Unexpected option '-%c'", c);
            MessageBoxW(NULL, szErrMsg, L"" PACKAGE, MB_OK | MB_ICONSTOP);
            return 1;
        }
        }
    }

    if (install_given) {
        DWORD dwRet = install(0);
#if defined(_WIN64)
        if (dwRet == ERROR_SUCCESS) {
            dwRet = install(KEY_WOW64_32KEY);
        }
#endif
        if (dwRet != ERROR_SUCCESS) {
            MessageBoxW(NULL,
                        dwRet == ERROR_ACCESS_DENIED
                            ? L"You must have administrator privileges to install Dr. Mingw as the "
                              L"default application debugger"
                            : L"Unexpected error when trying to install Dr. Mingw as the default "
                              L"application debugger",
                        L"DrMingw", MB_OK | MB_ICONERROR);
            return 1;
        }
        MessageBoxW(NULL, L"Dr. Mingw has been installed as the default application debugger",
                    L"DrMingw", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (uninstall_given) {
        DWORD dwRet = uninstall(0);
#if defined(_WIN64)
        if (dwRet == ERROR_SUCCESS) {
            dwRet = uninstall(KEY_WOW64_32KEY);
        }
#endif
        if (dwRet != ERROR_SUCCESS) {
            MessageBoxW(NULL,
                        dwRet == ERROR_ACCESS_DENIED
                            ? L"You must have administrator privileges to uninstall Dr. Mingw as "
                              L"the default application debugger"
                            : L"Unexpected error when trying to uninstall Dr. Mingw as the default "
                              L"application debugger",
                        L"DrMingw", MB_OK | MB_ICONERROR);
            return 1;
        }
        MessageBoxW(NULL, L"Dr. Mingw has been uninstalled", L"DrMingw", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    if (process_id_given) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        if (!ObtainSeDebugPrivilege())
            MessageBoxW(NULL,
                        L"An error occurred while obtaining debug privileges.\nDrMingw will not "
                        L"debug system processes.",
                        L"DrMingw", MB_OK | MB_ICONERROR);

        createDialog();

        _beginthread(debugThread, 0, (void *)(UINT_PTR)dwProcessId);

        return mainLoop();
    } else {
        help();
    }

    return 0;
}
