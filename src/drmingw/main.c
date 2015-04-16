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

#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>

#include <process.h>
#include <stdlib.h>

#include "getopt.h"
#include "debugger.h"
#include "symbols.h"
#include "dialog.h"
#include "errmsg.h"
#include "log.h"
#include "outdbg.h"


static int process_id_given = 0;    /* Whether process-id was given.  */
static int install_given = 0;    /* Whether install was given.  */
static int auto_given = 0;    /* Whether auto was given.  */
static int uninstall_given = 0;    /* Whether uninstall was given.  */

static DebugOptions debug_options;

static void
help(void)
{
    MessageBox(
        NULL,
        _T(
            "Usage: drmingw [OPTIONS]\r\n"
            "\r\n"
            "Options:\r\n"
            "  -h, --help\tPrint help and exit\r\n"
            "  -V, --version\tPrint version and exit\r\n"
            "  -i, --install\tInstall as the default JIT debugger\r\n"
            "  -a, --auto\tStart automatically (used with -i)\r\n"
            "  -u, --uninstall\tUninstall\r\n"
            "  -pLONG, --process-id=LONG\r\n"
            "\t\tAttach to the process with the given identifier\r\n"
            "  -eLONG, --event=LONG\r\n"
            "\t\tSignal an event after process is attached\r\n"
            "  -b, --breakpoint\tTreat debug breakpoints as exceptions\r\n"
            "  -v, --verbose\tVerbose output\r\n"
#ifndef NDEBUG
            "  -d, --debug\tDebug output\r\n"
#endif
        ),
        _T(PACKAGE),
        MB_OK | MB_ICONINFORMATION
    );
}


static DWORD
install(REGSAM samDesired)
{
    TCHAR szFile[MAX_PATH];

    if (!GetModuleFileName(NULL, szFile, MAX_PATH)) {
        return GetLastError();
    }
        
    TCHAR szFullCommand[MAX_PATH + 256];
    HKEY hKey;
    long lRet;
    DWORD dwDisposition;

    lstrcpy(szFullCommand, szFile);
    lstrcat(szFullCommand, _T (" -p %ld -e %ld"));
    if (debug_options.verbose_flag)
        lstrcat(szFullCommand, _T (" -v"));
    if (debug_options.breakpoint_flag)
        lstrcat(szFullCommand, _T (" -b"));
    if (debug_options.debug_flag)
        lstrcat(szFullCommand, _T (" -d"));

    lRet = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        _T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"),    // The AeDebug registry key.
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE | samDesired,
        NULL,
        &hKey,
        &dwDisposition
    );
    if (lRet != ERROR_SUCCESS) {
        return lRet;
    }

    // Write the Debugger value.
    lRet = RegSetValueEx(
        hKey,
        _T("Debugger"),    // The debugger value.
        0,
        REG_SZ,
        (CONST BYTE *) szFullCommand,
        lstrlen(szFullCommand)*sizeof(TCHAR)
    );
    if (lRet == ERROR_SUCCESS) {
        // Write the Auto value.
        lRet = RegSetValueEx(
            hKey,
            _T("Auto"),    // The auto value.
            0,
            REG_SZ,
            (CONST BYTE *)  (auto_given ? _T("1") : _T("0")),
            sizeof(TCHAR)
        );
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

    lRet = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        _T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"),    // The AeDebug registry key.
        0,
        KEY_ALL_ACCESS | samDesired,
        &hKey
    );
    if (lRet != ERROR_SUCCESS) {
        return lRet;
    }

    // Write the Debugger value.
    lRet = RegSetValueEx(
        hKey,
        _T("Debugger"),    // The debugger value.
        0,
        REG_SZ,
        (CONST BYTE *) _T(""),
        0
    );
    if (lRet == ERROR_SUCCESS) {
        // Write the Auto value.
        lRet = RegSetValueEx(
            hKey,
            _T("Auto"),    // The auto value.
            0,
            REG_SZ,
            (CONST BYTE *)  _T("0"),
            sizeof(TCHAR)
        );
    }

    // Close the key no matter what happened.
    RegCloseKey(hKey);

    return lRet;
}


static DWORD
getProcessIdByName(const char *szProcessName)
{
    DWORD dwProcessId = 0;

    HANDLE hProcessSnap;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof pe32;
        if (Process32First(hProcessSnap, &pe32)) {
            do {
                if (stricmp(szProcessName, pe32.szExeFile) == 0) {
                    dwProcessId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);
    }

    return dwProcessId;
}


static void debugThread(void *arg)
{
    // attach debuggee
    if (!DebugActiveProcess(debug_options.dwProcessId)) {
        ErrorMessageBox(_T("DebugActiveProcess: %s"), LastErrorMessage());
        return;
    }

    setDumpCallback(appendText);

    SetSymOptions(TRUE, debug_options.debug_flag);

    DebugMainLoop(&debug_options);
}



int
main(int argc, char **argv)
{
    int c;    /* Character of the parsed option.  */

    debug_options.first_chance = 1;

    while (1)
    {
        int option_index = 0;
        static const struct option long_options[] =
        {
            { "help", 0, NULL, 'h'},
            { "version", 0, NULL, 'V'},
            { "install", 0, NULL, 'i'},
            { "auto", 0, NULL, 'a'},
            { "uninstall", 0, NULL, 'u'},
            { "process-id", 1, NULL, 'p'},
            { "event", 1, NULL, 'e'},
            { "breakpoint", 0, NULL, 'b'},
            { "verbose", 0, NULL, 'v'},
            { "debug", 0, NULL, 'd'},
            { NULL, 0, NULL, 0}
        };

        c = getopt_long (argc, argv, "?hViaup:e:vbd", long_options, &option_index);

        if (c == -1)
            break;    /* Exit from `while (1)' loop.  */

        switch (c) {
            case '?':
                if (optopt != '?') {
                    /* Invalid option.  */
                    MessageBox(
                        NULL,
                        _T("Invalid option"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                /* fall-through */
            case 'h':    /* Print help and exit.  */
                help();
                return 0;

            case 'V':    /* Print version and exit.  */
                MessageBox(
                    NULL,
                    _T(PACKAGE " " VERSION),
                    _T(PACKAGE),
                    MB_OK | MB_ICONINFORMATION
                );
                return 0;

            case 'i':    /* Install as the default JIT debugger.  */
                if (uninstall_given)
                {
                    MessageBox(
                        NULL,
                        _T("conficting options `--uninstall' (`-u') and `--install' (`-i')"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                install_given = 1;
                break;

            case 'a':    /* Automatically start.  */
                if (uninstall_given)
                {
                    MessageBox(
                        NULL,
                        _T("conficting options `--uninstall' (`-u') and `--auto' (`-a')"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                auto_given = 1;
                break;

            case 'u':    /* Uninstall.  */
                if (install_given)
                {
                    MessageBox(
                        NULL,
                        _T("conficting options `--install' (`-i') and `--uninstall' (`-u')"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                if (auto_given)
                {
                    MessageBox(
                        NULL,
                        _T("conficting options `--auto' (`-a') and `--uninstall' (`-u')"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                uninstall_given = 1;
                break;

            case 'p':    /* Attach to the process with the given identifier.  */
                if (process_id_given)
                {
                    MessageBox(
                        NULL,
                        _T("`--process-id' (`-p') option redeclared"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                process_id_given = 1;
                if (optarg[0] >= '0' && optarg[0] <= '9') {
                    debug_options.dwProcessId = strtoul(optarg, NULL, 0);
                } else {
                    debug_options.breakpoint_flag = 1;
                    debug_options.dwProcessId = getProcessIdByName(optarg);
                }
                if (!debug_options.dwProcessId) {
                    MessageBox(
                        NULL,
                        _T("Invalid process"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                break;

            case 'e':    /* Signal an event after process is attached.  */
                if (debug_options.hEvent)
                {
                    MessageBox(
                        NULL,
                        _T("`--event' (`-e') option redeclared"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                debug_options.hEvent = (HANDLE) (INT_PTR) atol (optarg);
                break;

            case 'b':    /* Treat debug breakpoints as exceptions */
                debug_options.breakpoint_flag = 1;
                break;

            case 'v':    /* Verbose output.  */
                debug_options.verbose_flag = 1;
                break;

            case 'd':    /* Debug output.  */
                debug_options.debug_flag = 1;
                break;

            default:    /* bug: option not considered.  */
            {
                TCHAR szErrMsg[1024];

                wsprintf(szErrMsg, "'-%c' option unknown", c);

                MessageBox(
                    NULL,
                    szErrMsg,
                    _T(PACKAGE),
                    MB_OK | MB_ICONSTOP
                );
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
            MessageBox(
                NULL,
                dwRet == ERROR_ACCESS_DENIED
                ? _T("You must have administrator privileges to install Dr. Mingw as the default application debugger") 
                : _T("Unexpected error when trying to install Dr. Mingw as the default application debugger"),
                _T("DrMingw"),
                MB_OK | MB_ICONERROR
            );
            return 1;
        }
        MessageBox(
            NULL,
            _T("Dr. Mingw has been installed as the default application debugger"),
            _T("DrMingw"),
            MB_OK | MB_ICONINFORMATION
        );
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
            MessageBox(
                NULL,
                dwRet == ERROR_ACCESS_DENIED
                ? _T("You must have administrator privileges to uninstall Dr. Mingw as the default application debugger") 
                : _T("Unexpected error when trying to uninstall Dr. Mingw as the default application debugger"),
                _T("DrMingw"),
                MB_OK | MB_ICONERROR
            );
            return 1;
        }
        MessageBox(
            NULL,
            _T("Dr. Mingw has been uninstalled"),
            _T("DrMingw"),
            MB_OK | MB_ICONINFORMATION
        );
        return 0;
    }

    if (process_id_given) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        if(!ObtainSeDebugPrivilege())
            MessageBox(
                NULL,
                _T("An error occured while obtaining debug privileges.\nDrMingw will not debug system processes."),
                _T("DrMingw"),
                MB_OK | MB_ICONERROR
            );

        createDialog();

        _beginthread(debugThread, 0, NULL);

        return mainLoop();
    } else {
        help();
    }

    return 0;
}

