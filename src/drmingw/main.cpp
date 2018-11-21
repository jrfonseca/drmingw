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

#include "getopt.h"
#include "debugger.h"
#include "symbols.h"
#include "dialog.h"
#include "errmsg.h"
#include "log.h"
#include "outdbg.h"


static int process_id_given = 0;    /* Whether process-id was given.  */
static int install_given = 0;    /* Whether install was given.  */
static int uninstall_given = 0;    /* Whether uninstall was given.  */

static DebugOptions debug_options;

static void
help(void)
{
    MessageBoxA(
        NULL,
        "Usage: drmingw [OPTIONS]\r\n"
        "\r\n"
        "Options:\r\n"
        "  -h, --help\tPrint help and exit\r\n"
        "  -V, --version\tPrint version and exit\r\n"
        "  -i, --install\tInstall as the default JIT debugger\r\n"
        "  -u, --uninstall\tUninstall\r\n"
        "  -pLONG, --process-id=LONG\r\n"
        "\t\tAttach to the process with the given identifier\r\n"
        "  -eLONG, --event=LONG\r\n"
        "\t\tSignal an event after process is attached\r\n"
        "  -b, --breakpoint\tTreat debug breakpoints as exceptions\r\n"
        "  -v, --verbose\tVerbose output\r\n"
        "  -d, --debug\tDebug output\r\n"
        ,
        PACKAGE,
        MB_OK | MB_ICONINFORMATION
    );
}


static LSTATUS
regSetStr(HKEY hKey, LPCSTR lpValueName, LPCSTR szStr)
{
    return RegSetValueExA(
        hKey, lpValueName, 0, REG_SZ,
        reinterpret_cast<LPCBYTE>(szStr), strlen(szStr) + 1
    );
}


static DWORD
install(REGSAM samDesired)
{
    char szFile[MAX_PATH];
    if (!GetModuleFileNameA(NULL, szFile, MAX_PATH)) {
        return GetLastError();
    }
        
    std::string debuggerCommand;
    debuggerCommand += '"';
    debuggerCommand += szFile;
    debuggerCommand += "\" -p %ld -e %ld";
    if (debug_options.verbose_flag)
        debuggerCommand += " -v";
    if (debug_options.breakpoint_flag)
        debuggerCommand += " -b";
    if (debug_options.debug_flag)
        debuggerCommand += " -d";

    HKEY hKey;
    long lRet;
    DWORD dwDisposition;

    lRet = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug",    // The AeDebug registry key.
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
    lRet = regSetStr(hKey, "Debugger", debuggerCommand.c_str());
    if (lRet == ERROR_SUCCESS) {
        // Write the Auto value.
        lRet = regSetStr(hKey, "Auto", "1");
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

    lRet = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug",    // The AeDebug registry key.
        0,
        KEY_ALL_ACCESS | samDesired,
        &hKey
    );
    if (lRet != ERROR_SUCCESS) {
        return lRet;
    }

    // Write the Debugger value.
    lRet = regSetStr(hKey, "Debugger", "");

    // Leave Auto value as "1".  It is the default
    // (https://docs.microsoft.com/en-us/windows/desktop/Debug/configuring-automatic-debugging)
    // and setting it to "0" doesn't seem to work as documented on Windows 10
    // (https://github.com/jrfonseca/drmingw/issues/38)

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
    DWORD dwProcessId = (DWORD)(UINT_PTR)arg;

    // attach debuggee
    if (!DebugActiveProcess(dwProcessId)) {
        ErrorMessageBox("DebugActiveProcess: %s", LastErrorMessage());
        return;
    }

    setDumpCallback(appendText);

    SetSymOptions(debug_options.debug_flag);

    DebugMainLoop(&debug_options);
}



int
main(int argc, char **argv)
{
    DWORD dwProcessId = 0;
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
            { "uninstall", 0, NULL, 'u'},
            { "process-id", 1, NULL, 'p'},
            { "event", 1, NULL, 'e'},
            { "tid", 1, NULL, 't'},
            { "breakpoint", 0, NULL, 'b'},
            { "verbose", 0, NULL, 'v'},
            { "debug", 0, NULL, 'd'},
            { NULL, 0, NULL, 0}
        };

        c = getopt_long_only(argc, argv, "?hViup:e:t:vbd", long_options, &option_index);

        if (c == -1)
            break;    /* Exit from `while (1)' loop.  */

        switch (c) {
            case '?':
                if (optopt != '?') {
                    /* Invalid option.  */
                    char szErrMsg[512];
                    sprintf(szErrMsg, "Invalid option '%c'", optopt);
                    MessageBoxA(
                        NULL,
                        szErrMsg,
                        PACKAGE,
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                /* fall-through */
            case 'h':    /* Print help and exit.  */
                help();
                return 0;

            case 'V':    /* Print version and exit.  */
                MessageBoxA(
                    NULL,
                    PACKAGE " " VERSION,
                    PACKAGE,
                    MB_OK | MB_ICONINFORMATION
                );
                return 0;

            case 'i':    /* Install as the default JIT debugger.  */
                if (uninstall_given)
                {
                    MessageBoxA(
                        NULL,
                        "conficting options `--uninstall' (`-u') and `--install' (`-i')",
                        PACKAGE,
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                install_given = 1;
                break;

            case 'u':    /* Uninstall.  */
                if (install_given)
                {
                    MessageBoxA(
                        NULL,
                        "conficting options `--install' (`-i') and `--uninstall' (`-u')",
                        PACKAGE,
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                uninstall_given = 1;
                break;

            case 'p':    /* Attach to the process with the given identifier.  */
                if (process_id_given)
                {
                    MessageBoxA(
                        NULL,
                        "`--process-id' (`-p') option redeclared",
                        PACKAGE,
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                process_id_given = 1;
                if (optarg[0] >= '0' && optarg[0] <= '9') {
                    dwProcessId = strtoul(optarg, NULL, 0);
                } else {
                    debug_options.breakpoint_flag = 1;
                    dwProcessId = getProcessIdByName(optarg);
                }
                if (!dwProcessId) {
                    MessageBoxA(
                        NULL,
                        "Invalid process",
                        PACKAGE,
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                break;

            case 'e':    /* Signal an event after process is attached.  */
                if (debug_options.hEvent)
                {
                    MessageBoxA(
                        NULL,
                        "`--event' (`-e') option redeclared",
                        PACKAGE,
                        MB_OK | MB_ICONSTOP
                    );
                    return 1;
                }
                debug_options.hEvent = (HANDLE) (INT_PTR) atol (optarg);
                break;

            case 't': {
                /* Thread id.  Used when debugging WinRT apps. */
                debug_options.dwThreadId = strtoul(optarg, NULL, 0);
                break;
            }

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
                char szErrMsg[512];
                sprintf(szErrMsg, "Unexpected option '-%c'", c);
                MessageBoxA(
                    NULL,
                    szErrMsg,
                    PACKAGE,
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
            MessageBoxA(
                NULL,
                dwRet == ERROR_ACCESS_DENIED
                ? "You must have administrator privileges to install Dr. Mingw as the default application debugger" 
                : "Unexpected error when trying to install Dr. Mingw as the default application debugger",
                "DrMingw",
                MB_OK | MB_ICONERROR
            );
            return 1;
        }
        MessageBoxA(
            NULL,
            "Dr. Mingw has been installed as the default application debugger",
            "DrMingw",
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
            MessageBoxA(
                NULL,
                dwRet == ERROR_ACCESS_DENIED
                ? "You must have administrator privileges to uninstall Dr. Mingw as the default application debugger" 
                : "Unexpected error when trying to uninstall Dr. Mingw as the default application debugger",
                "DrMingw",
                MB_OK | MB_ICONERROR
            );
            return 1;
        }
        MessageBoxA(
            NULL,
            "Dr. Mingw has been uninstalled",
            "DrMingw",
            MB_OK | MB_ICONINFORMATION
        );
        return 0;
    }

    if (process_id_given) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        if(!ObtainSeDebugPrivilege())
            MessageBoxA(
                NULL,
                "An error occurred while obtaining debug privileges.\nDrMingw will not debug system processes.",
                "DrMingw",
                MB_OK | MB_ICONERROR
            );

        createDialog();

        _beginthread(debugThread, 0, (void *)(UINT_PTR)dwProcessId);

        return mainLoop();
    } else {
        help();
    }

    return 0;
}

