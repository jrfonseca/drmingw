/* main.c
 *
 *
 * Jose Fonseca <j_r_fonseca@yahoo.co.uk>
 */

#include <assert.h>

#include <windows.h>
#include <tchar.h>

#include <process.h>
#include <stdlib.h>

#include "getopt.h"
#include "debugger.h"
#include "dialog.h"
#include "log.h"
#include "misc.h"


static int process_id_given = 0;    /* Whether process-id was given.  */
static int install_given = 0;    /* Whether install was given.  */
static int auto_given = 0;    /* Whether auto was given.  */
static int uninstall_given = 0;    /* Whether uninstall was given.  */


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
        ),
        _T(PACKAGE),
        MB_OK | MB_ICONINFORMATION
    );
}


static DWORD
install(void)
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
    if(verbose_flag)
        lstrcat(szFullCommand, _T (" -v"));
    if(breakpoint_flag)
        lstrcat(szFullCommand, _T (" -b"));

    lRet = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        _T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"),    // The AeDebug registry key.
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
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
uninstall(void)
{
    HKEY hKey;
    long lRet;

    lRet = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        _T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"),    // The AeDebug registry key.
        0,
        KEY_ALL_ACCESS,
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


int main (int argc, char **argv)
{
    int c;    /* Character of the parsed option.  */

    while (1)
    {
        int option_index = 0;
        static const struct option long_options[] =
        {
            { "help", 0, NULL, 'h'},
            { "version", 0, NULL, 'V'},
            { "install", 0, NULL, 'i'},
            { "auto", 0, NULL, 'a'},
            { "install", 0, NULL, 'i'},
            { "uninstall", 0, NULL, 'u'},
            { "process-id", 1, NULL, 'p'},
            { "event", 1, NULL, 'e'},
            { "breakpoint", 0, NULL, 'b'},
            { "verbose", 0, NULL, 'v'},
            { NULL, 0, NULL, 0}
        };

        c = getopt_long (argc, argv, "?hViaup:e:vb", long_options, &option_index);

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
                    return 0;
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
                if (install_given)
                {
                    MessageBox(
                        NULL,
                        _T("`--install' (`-i') option redeclared"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
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
                if (auto_given)
                {
                    MessageBox(
                        NULL,
                        _T("`--auto' (`-a') option redeclared"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
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
                if (uninstall_given)
                {
                    MessageBox(
                        NULL,
                        _T("`--uninstall' (`-u') option redeclared"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
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
                    return 0;
                }
                process_id_given = 1;
                dwProcessId = strtoul (optarg, NULL, 0);
                break;

            case 'e':    /* Signal an event after process is attached.  */
                if (hEvent)
                {
                    MessageBox(
                        NULL,
                        _T("`--event' (`-e') option redeclared"),
                        _T(PACKAGE),
                        MB_OK | MB_ICONSTOP
                    );
                    return 0;
                }
                hEvent = (HANDLE) (INT_PTR) atol (optarg);
                break;

            case 'b':    /* Treat debug breakpoints as exceptions */
                breakpoint_flag = 1;
                break;

            case 'v':    /* Verbose output.  */
                verbose_flag = 1;
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
                return 0;
            }
        }
    }

    if (install_given) {
        DWORD dwRet = install();
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
        DWORD dwRet = uninstall();
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

        _beginthread(DebugProcess, 0, NULL);

        Dialog();
    } else {
        help();
    }

    return 0;
}

